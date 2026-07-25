#include <unity.h>

#include "OfflineBuffer.h"
#include "SystemConfig.h"
#include "UplinkScheduler.h"
#include "VehicleData.h"

// ===========================================================================
// M1 — UplinkScheduler (saf) native testleri. aks_loop_sim.py'deki temel
// senaryoyu (link düşer → 1 Hz offline örnekleme → link gelir → tik başına
// 1 replay + 1 canlı drenaj) GERÇEK C++ sınıfı üzerinde birebir kurar; böylece
// Python modeliyle firmware kodu arasındaki ayrışma riski kapanır.
// ===========================================================================

namespace {

// Gönderim kaydı (AUX'i her zaman hazır varsayar — Python modeliyle aynı).
struct Emitted {
    bool isReplay;
    uint32_t ts;
};
Emitted g_emitted[1024];
int g_emittedCount = 0;
bool g_sendAlwaysOk = true;  // false → AUX meşgul simülasyonu

bool recordSend(const TelemetryData& pkt, bool isReplay, void* /*ctx*/) {
    if (!g_sendAlwaysOk) return false;
    if (g_emittedCount < 1024)
        g_emitted[g_emittedCount++] = {isReplay, pkt.TEL_timestampMs};
    return true;
}

TelemetryData mkPacket(uint32_t ts) {
    TelemetryData d{};
    d.TEL_timestampMs = ts;
    return d;
}

UplinkScheduler makeScheduler() {
    return UplinkScheduler(LINK_TIMEOUT_MS, BOOT_LINK_GRACE_MS,
                           OFFLINE_SAMPLE_PERIOD_MS, REPLAY_BURST_PER_TICK,
                           LORA_UNKNOWN_BYTE_WARN_INTERVAL_MS);
}

void resetEnv() {
    ob_reset();
    g_emittedCount = 0;
    g_sendAlwaysOk = true;
}

}  // namespace

// ---------------------------------------------------------------------------
// onRxByte: heartbeat / bilinmeyen (throttled WARN)
// ---------------------------------------------------------------------------
void test_rx_heartbeat_is_classified(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(UplinkScheduler::RxResult::HEARTBEAT),
        static_cast<int>(s.onRxByte(UKS_HEARTBEAT_BYTE, 1000)));
}

void test_rx_unknown_warns_once_then_quiet(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    const uint8_t junk = UKS_HEARTBEAT_BYTE ^ 0xFF;
    // lora_note_unknown_byte: last_warn 0'dan başlar → ilk WARN ancak
    // now >= interval olduğunda. (Sayaç her byte'ta artar.)
    const uint64_t t0 = LORA_UNKNOWN_BYTE_WARN_INTERVAL_MS;  // ilk WARN anı
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(UplinkScheduler::RxResult::UNKNOWN_WARN),
        static_cast<int>(s.onRxByte(junk, t0)));
    // interval içinde ikincisi → QUIET
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(UplinkScheduler::RxResult::UNKNOWN_QUIET),
        static_cast<int>(s.onRxByte(junk, t0 + 5)));
    // interval geçtikten sonra tekrar WARN
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(UplinkScheduler::RxResult::UNKNOWN_WARN),
        static_cast<int>(s.onRxByte(junk, t0 + LORA_UNKNOWN_BYTE_WARN_INTERVAL_MS)));
    TEST_ASSERT_EQUAL_UINT32(3u, s.unknownByteCount());
}

// ---------------------------------------------------------------------------
// Boot-grace: hiç heartbeat gelmezse boot'tan BOOT_LINK_GRACE_MS sonra DOWN.
// ---------------------------------------------------------------------------
void test_boot_grace_link_down_when_no_heartbeat(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    const uint64_t bootMs = 0;

    // grace içinde: link UP (henüz DOWN değil)
    UplinkScheduler::LinkTransition tr = s.updateLink(BOOT_LINK_GRACE_MS - 1, bootMs);
    TEST_ASSERT_FALSE(tr.becameDown);
    TEST_ASSERT_FALSE(s.isLinkDown());

    // grace geçti, hâlâ heartbeat yok → DOWN
    tr = s.updateLink(BOOT_LINK_GRACE_MS + 1, bootMs);
    TEST_ASSERT_TRUE(tr.becameDown);
    TEST_ASSERT_TRUE(s.isLinkDown());
}

// ---------------------------------------------------------------------------
// onTxTickLinkUp: AUX meşgulse (send false) replay düşürülmez, canlı atlanır.
// ---------------------------------------------------------------------------
void test_tx_tick_aux_busy_keeps_buffer(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    ob_push(mkPacket(111));
    ob_push(mkPacket(222));

    g_sendAlwaysOk = false;  // AUX meşgul
    int sent = s.onTxTickLinkUp(true, mkPacket(999), &recordSend, nullptr);
    TEST_ASSERT_EQUAL_INT(0, sent);
    TEST_ASSERT_EQUAL_INT(2, ob_count());  // hiçbir paket düşmedi
    TEST_ASSERT_EQUAL_INT(0, g_emittedCount);
}

// ---------------------------------------------------------------------------
// onTxTickLinkUp: tik başına <=REPLAY_BURST replay + 1 canlı; replay FIFO.
// ---------------------------------------------------------------------------
void test_tx_tick_one_replay_then_live(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    ob_push(mkPacket(10));
    ob_push(mkPacket(20));

    int sent = s.onTxTickLinkUp(true, mkPacket(999), &recordSend, nullptr);
    // REPLAY_BURST_PER_TICK=1 → 1 replay + 1 live
    TEST_ASSERT_EQUAL_INT(REPLAY_BURST_PER_TICK + 1, sent);
    TEST_ASSERT_EQUAL_INT(2, g_emittedCount);
    TEST_ASSERT_TRUE(g_emitted[0].isReplay);
    TEST_ASSERT_EQUAL_UINT32(10u, g_emitted[0].ts);  // en eski önce
    TEST_ASSERT_FALSE(g_emitted[1].isReplay);
    TEST_ASSERT_EQUAL_UINT32(999u, g_emitted[1].ts);  // canlı
    TEST_ASSERT_EQUAL_INT(1, ob_count());  // 20 hâlâ tamponda
}

// ---------------------------------------------------------------------------
// SENARYO: link UP → outage (1 Hz offline örnekleme) → recovery → replay drenajı.
// aks_loop_sim.run_outage_simulation'ın C++ eşleniği.
// ---------------------------------------------------------------------------
void test_outage_offline_sampling_then_replay_drain(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    const uint64_t bootMs = 0;

    uint64_t now = 1000;
    s.onRxByte(UKS_HEARTBEAT_BYTE, now);
    s.updateLink(now, bootMs);
    TEST_ASSERT_FALSE(s.isLinkDown());
    
    uint32_t lastLiveTs = 0;
    for (int i = 0; i < 15; i++) {
        now += 1000;
        s.onRxByte(UKS_HEARTBEAT_BYTE, now);
        s.updateLink(now, bootMs);
        s.recordLookback(now, mkPacket((uint32_t)now));
        s.onTxTickLinkUp(true, mkPacket((uint32_t)now), &recordSend, nullptr);
        lastLiveTs = (uint32_t)now;
    }

    for (int i = 0; i < LINK_TIMEOUT_MS / 1000 + 1; i++) {
        now += 1000;
        s.recordLookback(now, mkPacket((uint32_t)now));
        UplinkScheduler::LinkTransition tr = s.updateLink(now, bootMs);
        if (tr.becameDown) {
            break;
        }
    }
    TEST_ASSERT_TRUE(s.isLinkDown());

    const int OUTAGE_S = 10;
    uint32_t firstTs = 0, lastTs = 0;
    int sampleCount = ob_count();
    for (int i = 0; i < OUTAGE_S; i++) {
        now += OFFLINE_SAMPLE_PERIOD_MS;
        s.updateLink(now, bootMs);
        if (s.offlineSample(now, mkPacket((uint32_t)now))) {
            sampleCount++;
        }
    }
    TEST_ASSERT_EQUAL_INT(sampleCount, ob_count());

    const uint64_t recovMs = now + OFFLINE_SAMPLE_PERIOD_MS;
    s.onRxByte(UKS_HEARTBEAT_BYTE, recovMs);
    UplinkScheduler::LinkTransition up = s.updateLink(recovMs, bootMs);
    TEST_ASSERT_TRUE(up.becameUp);
    TEST_ASSERT_TRUE(up.hadSamples);
    
    uint32_t firstOfflineTs = up.firstTs;
    uint32_t diff = firstOfflineTs > lastLiveTs ? firstOfflineTs - lastLiveTs : lastLiveTs - firstOfflineTs;
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(5000u, diff);

    g_emittedCount = 0;
    uint64_t txt = recovMs;
    for (int i = 0; i < sampleCount + 2; i++) {
        s.onTxTickLinkUp(true, mkPacket(50000u + (uint32_t)i), &recordSend, nullptr);
        txt += LORA_TX_PERIOD_MS;
    }

    TEST_ASSERT_EQUAL_INT(0, ob_count());
}
