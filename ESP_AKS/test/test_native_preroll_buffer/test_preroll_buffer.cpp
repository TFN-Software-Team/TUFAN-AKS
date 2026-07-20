#include <unity.h>

#include "PrerollBuffer.h"
#include "SystemConfig.h"
#include "VehicleData.h"

// ===========================================================================
// R2 — PrerollBuffer (saf) native testleri. Link durumundan bağımsız sürekli
// 1 Hz örnekleme + dairesel taşma + FIFO aktarım (splice hazırlığı).
// bkz. test_native_uplink_scheduler için DOWN-geçişi splice entegrasyon
// testleri (test_preroll_splices_into_offline_buffer_on_down_with_order,
// test_short_outage_near_detection_threshold_covered_by_preroll).
// ===========================================================================

namespace {

TelemetryData mkPacket(uint32_t ts) {
    TelemetryData d{};
    d.TEL_timestampMs = ts;
    return d;
}

const uint32_t PERIOD = 1000u;

}  // namespace

void test_preroll_first_sample_is_immediate(void) {
    PrerollBuffer pb;
    TEST_ASSERT_TRUE(pb.sample(1000, mkPacket(1000), PERIOD));
    TEST_ASSERT_EQUAL_INT(1, pb.count());
}

void test_preroll_throttles_within_period(void) {
    PrerollBuffer pb;
    TEST_ASSERT_TRUE(pb.sample(1000, mkPacket(1000), PERIOD));
    // period dolmadan ikinci çağrı örnek almaz.
    TEST_ASSERT_FALSE(pb.sample(1500, mkPacket(1500), PERIOD));
    TEST_ASSERT_EQUAL_INT(1, pb.count());
}

void test_preroll_samples_at_and_past_period(void) {
    PrerollBuffer pb;
    TEST_ASSERT_TRUE(pb.sample(1000, mkPacket(1000), PERIOD));
    TEST_ASSERT_TRUE(pb.sample(2000, mkPacket(2000), PERIOD));  // tam period
    TEST_ASSERT_TRUE(pb.sample(3200, mkPacket(3200), PERIOD));  // period aşıldı
    TEST_ASSERT_EQUAL_INT(3, pb.count());
}

void test_preroll_pop_oldest_fifo_order(void) {
    PrerollBuffer pb;
    pb.sample(1000, mkPacket(1000), PERIOD);
    pb.sample(2000, mkPacket(2000), PERIOD);
    pb.sample(3000, mkPacket(3000), PERIOD);

    TelemetryData out;
    TEST_ASSERT_TRUE(pb.popOldest(out));
    TEST_ASSERT_EQUAL_UINT32(1000u, out.TEL_timestampMs);
    TEST_ASSERT_TRUE(pb.popOldest(out));
    TEST_ASSERT_EQUAL_UINT32(2000u, out.TEL_timestampMs);
    TEST_ASSERT_TRUE(pb.popOldest(out));
    TEST_ASSERT_EQUAL_UINT32(3000u, out.TEL_timestampMs);
    TEST_ASSERT_EQUAL_INT(0, pb.count());
}

void test_preroll_pop_oldest_empty_returns_false(void) {
    PrerollBuffer pb;
    TelemetryData out;
    TEST_ASSERT_FALSE(pb.popOldest(out));
}

// Kapasite = LINK_TIMEOUT_MS/1000 + 2 (derleme zamanı türetme) — mevcut
// LINK_TIMEOUT_MS=9000 için 11 olmalı; LINK_TIMEOUT_MS değişirse bu test de
// otomatik senkron kalır (literal 11 yerine formülü de doğrular).
void test_preroll_capacity_matches_link_timeout_derivation(void) {
    TEST_ASSERT_EQUAL_UINT32(((LINK_TIMEOUT_MS) / 1000U) + 2U, PREROLL_CAPACITY);
}

// Kapasiteyi aşan örnekleme: en eski düşer, kalan PREROLL_CAPACITY kayıt
// kronolojik sırayı korur (son PREROLL_CAPACITY saniyenin penceresi).
void test_preroll_circulates_keeps_last_capacity_samples(void) {
    PrerollBuffer pb;
    const int totalSamples = (int)PREROLL_CAPACITY + 5;
    for (int i = 0; i < totalSamples; i++) {
        const uint64_t t = 1000ull + (uint64_t)i * PERIOD;
        TEST_ASSERT_TRUE(pb.sample(t, mkPacket((uint32_t)t), PERIOD));
    }
    TEST_ASSERT_EQUAL_INT((int)PREROLL_CAPACITY, pb.count());

    // İlk 5 örnek (i=0..4) düşmüş olmalı; kalanlar i=5..totalSamples-1,
    // en eskiden en yeniye doğru sırayla popOldest ile çıkmalı.
    const uint32_t expectedFirstTs = (uint32_t)(1000ull + 5ull * PERIOD);
    TelemetryData out;
    TEST_ASSERT_TRUE(pb.popOldest(out));
    TEST_ASSERT_EQUAL_UINT32(expectedFirstTs, out.TEL_timestampMs);

    uint32_t prevTs = out.TEL_timestampMs;
    int remaining = pb.count();
    for (int i = 0; i < remaining; i++) {
        TEST_ASSERT_TRUE(pb.popOldest(out));
        TEST_ASSERT_TRUE(out.TEL_timestampMs > prevTs);  // artan ts — sıra bozulmadı
        prevTs = out.TEL_timestampMs;
    }
    TEST_ASSERT_EQUAL_INT(0, pb.count());
}

void test_preroll_reset_clears_state(void) {
    PrerollBuffer pb;
    pb.sample(1000, mkPacket(1000), PERIOD);
    pb.sample(2000, mkPacket(2000), PERIOD);
    pb.reset();
    TEST_ASSERT_EQUAL_INT(0, pb.count());
    // reset sonrası ilk örnek yine "immediate" olmalı (has_sample sıfırlandı).
    TEST_ASSERT_TRUE(pb.sample(2050, mkPacket(2050), PERIOD));
    TEST_ASSERT_EQUAL_INT(1, pb.count());
}
