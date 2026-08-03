#include <unity.h>

#include "ResyncPolicy.h"  // HMI_RESYNC_FIELD_COUNT (tavan üst sınırı kanıtı)
#include "SystemConfig.h"
#include "UpdateThrottle.h"

// =========================================================================
// packv gösterim tavanı — UpdateThrottle.h
//
// SORUN (saha, 03.08.2026): şarjda `packv` saniyede ~10 kez değişip
// okunamıyordu (HMI_Task 10 Hz + deciV çözünürlükte charger ripple).
// Bu testler tavanın SAF karar mantığını doğrular; DisplayHMI'deki
// entegrasyon (force yollarının muaf olması, atlanan tikte cache'in
// korunması) donanım gerektirdiği için burada DEĞİL, davranış olarak
// UpdateThrottle.h yorumlarında sözleşmelenir.
// =========================================================================

void test_throttle_first_call_is_due(void) {
    // lastSendTick=0, now=0 → (0-0)=0 >= 1000 DEĞİL, yani vadesi dolmamış.
    // Boot'ta ilk gönderimi sağlayan şey tavan değil forceFullRefresh'tir
    // (DisplayHMI::HMI_hasCachedScreen=false) — bu ayrım kasıtlıdır.
    TEST_ASSERT_FALSE(hmi_throttle_due(0, 0, 1000));
}

void test_throttle_blocks_before_interval(void) {
    TEST_ASSERT_FALSE(hmi_throttle_due(100, 0, 1000));
    TEST_ASSERT_FALSE(hmi_throttle_due(999, 0, 1000));
}

void test_throttle_due_exactly_at_interval(void) {
    // Sınır DAHİL (>=) — resync/freshness ile aynı semantik.
    TEST_ASSERT_TRUE(hmi_throttle_due(1000, 0, 1000));
    TEST_ASSERT_TRUE(hmi_throttle_due(1001, 0, 1000));
}

void test_throttle_zero_interval_disables_ceiling(void) {
    // 0 = tavan kapalı (eski davranışa dönüş yolu, CONFIG kaçışı).
    TEST_ASSERT_TRUE(hmi_throttle_due(0, 0, 0));
    TEST_ASSERT_TRUE(hmi_throttle_due(1, 0, 0));
}

void test_throttle_stamp_restarts_window(void) {
    uint32_t last = 0;
    TEST_ASSERT_TRUE(hmi_throttle_due(1000, last, 1000));
    hmi_throttle_stamp(1000, last);
    TEST_ASSERT_EQUAL_UINT32(1000, last);
    // Damgadan hemen sonra pencere yeniden kapanır.
    TEST_ASSERT_FALSE(hmi_throttle_due(1500, last, 1000));
    TEST_ASSERT_TRUE(hmi_throttle_due(2000, last, 1000));
}

void test_throttle_is_pure_query_does_not_mutate(void) {
    // Sorgu durumu DEĞİŞTİRMEZ — damgalama ayrı çağrıdır. Bu ayrım sayesinde
    // force/resync yollarından gelen gönderimler de pencereyi damgalayabilir.
    const uint32_t last = 500;
    hmi_throttle_due(9999, last, 1000);
    TEST_ASSERT_EQUAL_UINT32(500, last);
}

void test_throttle_tick_wraparound_safe(void) {
    // Sayaç taşması: lastSendTick tavana yakın, now sarmış.
    const uint32_t last = 0xFFFFFF00u;
    // last'tan sonra 0x100 (256) tick'te sayaç sarar; fark = 0x100 + now.
    TEST_ASSERT_FALSE(hmi_throttle_due(0xFFFFFF50u, last, 1000));  // fark 80
    TEST_ASSERT_FALSE(hmi_throttle_due(0x00000200u, last, 1000));  // fark 768
    TEST_ASSERT_TRUE(hmi_throttle_due(0x00000300u, last, 1000));   // fark 1024
}

void test_packv_interval_config_is_sane(void) {
    // Tavan HMI_Task periyodundan (100 ms) BÜYÜK olmalı — aksi halde hiçbir
    // etkisi olmaz ve şarjdaki zıplama geri döner.
    TEST_ASSERT_GREATER_THAN_UINT32(100u, HMI_PACKV_MIN_UPDATE_INTERVAL_MS);
    // Ve resync tam turundan (13 × 500 ms) küçük kalmalı: emniyet katmanı
    // alanı zaten zorla tazelediğinden daha büyük bir tavan anlamsızdır.
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        (uint32_t)HMI_RESYNC_FIELD_COUNT * HMI_RESYNC_INTERVAL_MS,
        HMI_PACKV_MIN_UPDATE_INTERVAL_MS);
}
