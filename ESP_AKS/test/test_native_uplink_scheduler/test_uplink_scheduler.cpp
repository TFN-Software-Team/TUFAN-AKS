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

    // --- Phase A: link UP (heartbeat), boş tampon → sadece canlı ---
    uint64_t now = 1000;  // t=0 sentineli değil
    s.onRxByte(UKS_HEARTBEAT_BYTE, now);
    s.updateLink(now, bootMs);
    TEST_ASSERT_FALSE(s.isLinkDown());
    int sent = s.onTxTickLinkUp(true, mkPacket((uint32_t)now), &recordSend, nullptr);
    TEST_ASSERT_EQUAL_INT(1, sent);  // tampon boş → yalnız canlı

    // --- Phase B: outage — heartbeat yok; LINK_TIMEOUT_MS sonra DOWN ---
    now = 1000 + LINK_TIMEOUT_MS + 1;
    UplinkScheduler::LinkTransition tr = s.updateLink(now, bootMs);
    TEST_ASSERT_TRUE(tr.becameDown);

    // 10 sn boyunca 1 Hz örnekle (OFFLINE_SAMPLE_PERIOD_MS adımı)
    const int OUTAGE_S = 10;
    uint32_t firstTs = 0, lastTs = 0;
    int sampleCount = 0;
    for (int i = 0; i < OUTAGE_S; i++) {
        const uint64_t t = now + (uint64_t)i * OFFLINE_SAMPLE_PERIOD_MS;
        s.updateLink(t, bootMs);  // hâlâ DOWN
        if (s.offlineSample(t, mkPacket((uint32_t)t))) {
            if (sampleCount == 0) firstTs = (uint32_t)t;
            lastTs = (uint32_t)t;
            sampleCount++;
        }
    }
    TEST_ASSERT_EQUAL_INT(OUTAGE_S, sampleCount);       // 1 Hz → 10 örnek
    TEST_ASSERT_EQUAL_INT(sampleCount, ob_count());

    // --- Phase C: recovery — heartbeat → UP; DOWN→UP raporu doğru ---
    const uint64_t recovMs = now + (uint64_t)OUTAGE_S * OFFLINE_SAMPLE_PERIOD_MS;
    s.onRxByte(UKS_HEARTBEAT_BYTE, recovMs);
    UplinkScheduler::LinkTransition up = s.updateLink(recovMs, bootMs);
    TEST_ASSERT_TRUE(up.becameUp);
    TEST_ASSERT_TRUE(up.hadSamples);
    TEST_ASSERT_EQUAL_INT(sampleCount, up.bufferedCount);
    TEST_ASSERT_EQUAL_UINT32(firstTs, up.firstTs);
    TEST_ASSERT_EQUAL_UINT32(lastTs, up.lastTs);

    // Drenaj: her tik 1 replay (en eski) + 1 canlı; tampon boşalana dek.
    g_emittedCount = 0;
    uint64_t txt = recovMs;
    for (int i = 0; i < sampleCount + 2; i++) {
        s.onTxTickLinkUp(true, mkPacket(50000u + (uint32_t)i), &recordSend, nullptr);
        txt += LORA_TX_PERIOD_MS;
    }

    // Tüm buffered paketler replay edildi, tampon boşaldı.
    TEST_ASSERT_EQUAL_INT(0, ob_count());
    int replaysSeen = 0, livesSeen = 0;
    uint32_t prevReplayTs = 0;
    bool replayOrderOk = true;
    for (int i = 0; i < g_emittedCount; i++) {
        if (g_emitted[i].isReplay) {
            if (replaysSeen > 0 && g_emitted[i].ts <= prevReplayTs)
                replayOrderOk = false;  // FIFO: ts artan
            prevReplayTs = g_emitted[i].ts;
            replaysSeen++;
        } else {
            livesSeen++;
        }
    }
    TEST_ASSERT_EQUAL_INT(sampleCount, replaysSeen);   // hepsi replay edildi
    TEST_ASSERT_TRUE(replayOrderOk);                   // en eski önce (FIFO)
    TEST_ASSERT_EQUAL_UINT32(firstTs, g_emitted[0].ts);  // ilk replay = ilk örnek
    TEST_ASSERT_EQUAL_INT(sampleCount + 2, livesSeen);   // her tikte 1 canlı
}

// ===========================================================================
// R2 — Pre-roll splice testleri (ADIM 2c). PrerollBuffer'ın kendi dolaşım/
// throttle davranışı test_native_preroll_buffer'da; burada yalnız DOWN
// geçişiyle OfflineBuffer'a entegrasyonu (splice) test edilir.
// ===========================================================================

// (ii) DOWN geçişinde pre-roll içeriği OfflineBuffer'ın önüne, kronolojik
// sırayla aktarılır.
void test_preroll_splices_into_offline_buffer_on_down_with_order(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    const uint64_t bootMs = 0;

    // Link UP + 5 sn boyunca pre-roll (heartbeat her saniye tazelenir, link
    // hiç DOWN olmaz — pre-roll link durumundan BAĞIMSIZ çalıştığını kanıtlar).
    for (int i = 1; i <= 5; i++) {
        const uint64_t t = (uint64_t)i * 1000u;
        s.onRxByte(UKS_HEARTBEAT_BYTE, t);
        s.updateLink(t, bootMs);
        s.prerollSample(t, mkPacket((uint32_t)t));
    }
    TEST_ASSERT_FALSE(s.isLinkDown());
    TEST_ASSERT_EQUAL_INT(0, ob_count());  // pre-roll OfflineBuffer'a DOKUNMADI (henüz splice yok)

    // Heartbeat kesilir; LINK_TIMEOUT_MS sonra DOWN tespit edilir (tespit
    // gecikmesi penceresi — bu sırada prerollSample() ÇAĞRILMADI, senaryo
    // sade tutuldu; kapasite/overflow ayrı testte).
    const uint64_t downMs = 5000u + LINK_TIMEOUT_MS + 1u;
    UplinkScheduler::LinkTransition tr = s.updateLink(downMs, bootMs);
    TEST_ASSERT_TRUE(tr.becameDown);
    TEST_ASSERT_EQUAL_INT(5, tr.prerollSplicedCount);
    TEST_ASSERT_EQUAL_INT(5, ob_count());

    // Kronolojik sıra korunmuş: 1000..5000 artan ts.
    TelemetryData out;
    for (uint32_t expected = 1000u; expected <= 5000u; expected += 1000u) {
        TEST_ASSERT_TRUE(ob_pop(out));
        TEST_ASSERT_EQUAL_UINT32(expected, out.TEL_timestampMs);
    }
    TEST_ASSERT_EQUAL_INT(0, ob_count());
}

// (iv) Splice sırasında OfflineBuffer taşarsa (zaten neredeyse dolu bir
// OfflineBuffer'a — ör. önceki bir flapping kesintiden kalan drenajsız
// kayıtlar — pre-roll eklenirse) en eski OfflineBuffer kaydı düşer, sıra
// bozulmaz (ob_push'ın var olan davranışının splice akışıyla bütünlüğü).
void test_preroll_splice_overflow_drops_oldest_keeps_order(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    const uint64_t bootMs = 0;

    // OfflineBuffer'ı OB_CAPACITY-2'ye kadar doldur (ts 1..OB_CAPACITY-2,
    // eskiden yeniye artan).
    const int PRE_FILL = OB_CAPACITY - 2;
    for (int i = 1; i <= PRE_FILL; i++) {
        ob_push(mkPacket((uint32_t)i));
    }
    TEST_ASSERT_EQUAL_INT(PRE_FILL, ob_count());

    // Pre-roll'a 5 YENİ (mevcut ob içeriğinden daha büyük ts'li) kayıt ekle.
    const uint32_t prerollBase = (uint32_t)PRE_FILL + 1000u;  // kesin daha büyük
    for (int i = 0; i < 5; i++) {
        const uint64_t t = 100000u + (uint64_t)i * 1000u;
        s.onRxByte(UKS_HEARTBEAT_BYTE, t);
        s.updateLink(t, bootMs);
        s.prerollSample(t, mkPacket(prerollBase + (uint32_t)i));
    }

    const uint64_t downMs = 100000u + 5000u + LINK_TIMEOUT_MS + 1u;
    UplinkScheduler::LinkTransition tr = s.updateLink(downMs, bootMs);
    TEST_ASSERT_TRUE(tr.becameDown);
    TEST_ASSERT_EQUAL_INT(5, tr.prerollSplicedCount);

    // OB_CAPACITY'yi aştı (PRE_FILL + 5 > OB_CAPACITY) → en eski (PRE_FILL+5-OB_CAPACITY)
    // kayıt düştü, count OB_CAPACITY'de sabitlendi.
    TEST_ASSERT_EQUAL_INT(OB_CAPACITY, ob_count());

    const int dropped = PRE_FILL + 5 - OB_CAPACITY;
    TelemetryData out;
    uint32_t prevTs = 0;
    uint32_t firstTs = 0;
    bool first = true;
    int poppedCount = 0;
    while (ob_pop(out)) {
        if (first) {
            firstTs = out.TEL_timestampMs;
        } else {
            TEST_ASSERT_TRUE(out.TEL_timestampMs > prevTs);  // sıra bozulmadı
        }
        prevTs = out.TEL_timestampMs;
        first = false;
        poppedCount++;
    }
    TEST_ASSERT_EQUAL_INT(OB_CAPACITY, poppedCount);
    // En eski kalan kayıt, düşenlerden hemen sonraki orijinal ts olmalı.
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(dropped + 1), firstTs);
    // Son kayıt, pre-roll'un son elemanı olmalı.
    TEST_ASSERT_EQUAL_UINT32(prerollBase + 4u, prevTs);
}

// (iii) Kısa (fiili LINK_TIMEOUT_MS civarı) kesinti: resmi DOWN süresi çok
// kısa olsa da (burada 1 tick), pre-roll tespit-öncesi ~LINK_TIMEOUT_MS
// penceresini geri doldurduğu için toplam kayıt aralığında (spliced +
// resmi offline örnek boyunca) hiçbir yerde 2*TICK'i (2 sn) aşan boşluk
// kalmaz — 9.2.h'nin "5 sn'yi aşan boşluk olmaz" kuralı kesintinin EN
// BAŞINDA da (yalnız sonunda değil) sağlanır. KARŞILAŞTIRMA (bkz.
// test_outage_offline_sampling_then_replay_drain, Phase B): pre-roll OLMASA
// bu senaryoda offlineSample() yalnızca resmi DOWN penceresinde (burada tek
// örnek) veri toplardı — kesintinin GERÇEK başlangıcından (~LINK_TIMEOUT_MS
// önce) o tek kayda kadar HİÇBİR veri olmazdı.
void test_short_outage_near_detection_threshold_covered_by_preroll(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    const uint64_t bootMs = 0;
    const uint64_t TICK = 1000u;
    uint64_t now = 0;

    // Steady-state link UP (30 sn) — pre-roll dolaşımda, kapasiteye ulaşır.
    for (int i = 0; i < 30; i++) {
        now += TICK;
        s.onRxByte(UKS_HEARTBEAT_BYTE, now);
        s.updateLink(now, bootMs);
        s.prerollSample(now, mkPacket((uint32_t)now));
    }
    TEST_ASSERT_FALSE(s.isLinkDown());

    // Heartbeat kesilir; pre-roll KOŞULSUZ devam eder — gerçek main.cpp'de
    // (ADIM 2a) her tick, link durumuna BAKILMAKSIZIN prerollSample() çağrılır.
    // DOWN tespit edilene kadar (link_check_timeout: '>' katı — bkz.
    // LinkMonitor.h) döngü sürer.
    UplinkScheduler::LinkTransition trDown;
    for (;;) {
        now += TICK;
        trDown = s.updateLink(now, bootMs);
        if (trDown.becameDown) break;
        s.prerollSample(now, mkPacket((uint32_t)now));
    }
    TEST_ASSERT_TRUE(trDown.becameDown);
    TEST_ASSERT_EQUAL_INT((int)PREROLL_CAPACITY, trDown.prerollSplicedCount);
    TEST_ASSERT_EQUAL_INT((int)PREROLL_CAPACITY, ob_count());
    const uint64_t downMs = now;

    // Aynı tick'te main.cpp sırasıyla aynı: updateLink() SONRASI prerollSample()
    // yine çağrılır (preroll bu tick'e ait YENİ bir kayıtla sıfırdan birikmeye
    // başlar — henüz splice edilmez, bir SONRAKİ DOWN geçişini bekler).
    s.prerollSample(now, mkPacket((uint32_t)now));

    // Resmi DOWN penceresi KISA sürer: yalnız 1 offline örnek alınır (throttle
    // nedeniyle aynı tick'te değil, bir SONRAKİ tick'te), sonra heartbeat geri gelir.
    now += TICK;
    TEST_ASSERT_TRUE(s.offlineSample(now, mkPacket((uint32_t)now)));
    const uint64_t firstOfficialOfflineTs = now;

    now += TICK;
    s.onRxByte(UKS_HEARTBEAT_BYTE, now);
    UplinkScheduler::LinkTransition trUp = s.updateLink(now, bootMs);
    TEST_ASSERT_TRUE(trUp.becameUp);
    TEST_ASSERT_TRUE(trUp.hadSamples);
    TEST_ASSERT_EQUAL_INT((int)PREROLL_CAPACITY + 1, trUp.bufferedCount);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)firstOfficialOfflineTs, trUp.lastTs);

    // KANIT (9.2.h — "5 sn'yi aşan boşluk olmaz", kesintinin EN BAŞINDA da):
    // OfflineBuffer'daki TÜM kayıtları (spliced + resmi offline örnek) FIFO
    // sırayla dolaş; ardışık iki kayıt arasındaki ts farkı hiçbir yerde
    // 2*TICK'i (2 sn) aşmamalı. (Spliced son kayıt ile ilk resmi offline
    // örnek arasında TAM 2*TICK'lik bir boşluk BEKLENİR — downMs anındaki tek
    // tick throttle nedeniyle atlanır: splice nowMs=downMs'i "son örnek anı"
    // yapar, o yüzden offlineSample(downMs,...) hemen tekrar örnek almaz —
    // bkz. test_preroll_splice_no_ts_duplicate_at_boundary; ilk başarılı
    // offline örnek ancak downMs+TICK'te gelir.) 2 sn << 5 sn (9.2.h marjı).
    //
    // KARŞILAŞTIRMA: pre-roll OLMASAYDI (bkz.
    // test_outage_offline_sampling_then_replay_drain, Phase B) tek kayıt
    // firstOfficialOfflineTs'den başlardı — kesintinin GERÇEK başlangıcından
    // (downMs'e kadar geçen ~LINK_TIMEOUT_MS) bu yana HİÇBİR veri olmazdı.
    TelemetryData out;
    uint32_t prevTs = 0;
    bool first = true;
    int count = 0;
    while (ob_pop(out)) {
        if (!first) {
            const uint32_t gap = out.TEL_timestampMs - prevTs;
            TEST_ASSERT_TRUE(gap > 0u);                  // tekrar yok
            TEST_ASSERT_TRUE(gap <= 2u * (uint32_t)TICK);  // >5 sn boşluk yok (gerçekte <=2 sn)
        }
        prevTs = out.TEL_timestampMs;
        first = false;
        count++;
    }
    TEST_ASSERT_EQUAL_INT((int)PREROLL_CAPACITY + 1, count);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)firstOfficialOfflineTs, prevTs);
}

// Splice sınırında (son pre-roll kaydı ile ilk yeni offlineSample arasında)
// ts tekrarı OLMAMALI — throttle, splice anının nowMs'ini devam ettirdiği
// için ilk offlineSample çağrısı (aynı tick'te gelse bile) örnek almaz.
void test_preroll_splice_no_ts_duplicate_at_boundary(void) {
    resetEnv();
    UplinkScheduler s = makeScheduler();
    const uint64_t bootMs = 0;

    // Boot-grace yolunu kullan (heartbeat hiç gelmesin) — sade DOWN tetikleme.
    for (int i = 1; i <= 5; i++) {
        const uint64_t t = (uint64_t)i * 1000u;
        s.prerollSample(t, mkPacket((uint32_t)t));
    }
    const uint64_t downMs = BOOT_LINK_GRACE_MS + 1u;  // 5001
    UplinkScheduler::LinkTransition tr = s.updateLink(downMs, bootMs);
    TEST_ASSERT_TRUE(tr.becameDown);
    TEST_ASSERT_EQUAL_INT(5, tr.prerollSplicedCount);
    TEST_ASSERT_EQUAL_INT(5, ob_count());

    // Aynı tick'te (nowMs=downMs) offlineSample çağrılsa bile throttle
    // nedeniyle ÖRNEK ALINMAZ (duplicate riski yok).
    TEST_ASSERT_FALSE(s.offlineSample(downMs, mkPacket((uint32_t)downMs)));
    TEST_ASSERT_EQUAL_INT(5, ob_count());

    // period_ms sonra normal örnekleme devam eder.
    const uint64_t nextMs = downMs + OFFLINE_SAMPLE_PERIOD_MS;
    TEST_ASSERT_TRUE(s.offlineSample(nextMs, mkPacket((uint32_t)nextMs)));
    TEST_ASSERT_EQUAL_INT(6, ob_count());

    // Tüm dizi kesin artan, tekrarsız: 1000,2000,3000,4000,5000,(downMs+period).
    TelemetryData out;
    uint32_t prevTs = 0;
    bool first = true;
    while (ob_pop(out)) {
        if (!first) TEST_ASSERT_TRUE(out.TEL_timestampMs > prevTs);
        prevTs = out.TEL_timestampMs;
        first = false;
    }
    TEST_ASSERT_EQUAL_UINT32((uint32_t)nextMs, prevTs);
}
