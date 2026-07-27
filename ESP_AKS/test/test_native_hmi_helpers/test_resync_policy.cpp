#include <unity.h>

#include <cstdint>

#include "ResyncPolicy.h"

// Round-robin resync politikası (ResyncPolicy.h) — reset dedektörünün RX
// hattı güvenilmezken devreye giren periyodik emniyet katmanı. Firmware'de
// tick = FreeRTOS tick, interval = pdMS_TO_TICKS(HMI_RESYNC_INTERVAL_MS);
// testte birimler ms olarak simüle edilir (politika birim-agnostik).
namespace {
constexpr uint32_t INTERVAL = 500;   // HMI_RESYNC_INTERVAL_MS varsayılanı
constexpr uint32_t TASK_PERIOD = 100;  // HMI_Task 10 Hz
}  // namespace

// Vade dolmadan çağrı: -1 döner ve durum (tick/sıra) DEĞİŞMEZ.
void test_resync_not_due_returns_no_field(void) {
    uint32_t last = 1000;
    uint8_t next = 3;
    TEST_ASSERT_EQUAL_INT(-1, hmi_resync_due_field(1000 + INTERVAL - 1, last,
                                                   next,
                                                   HMI_RESYNC_FIELD_COUNT,
                                                   INTERVAL));
    TEST_ASSERT_EQUAL_UINT32(1000, last);
    TEST_ASSERT_EQUAL_UINT8(3, next);
}

// Aynı tick'te ikinci çağrı alan DÖNDÜRMEZ — tetik başına tek alan (burst
// koruması: UART bütçesi tek komutla sınırlı kalır).
void test_resync_single_field_per_trigger(void) {
    uint32_t last = 0;
    uint8_t next = 0;
    TEST_ASSERT_EQUAL_INT(0, hmi_resync_due_field(INTERVAL, last, next,
                                                  HMI_RESYNC_FIELD_COUNT,
                                                  INTERVAL));
    TEST_ASSERT_EQUAL_INT(-1, hmi_resync_due_field(INTERVAL, last, next,
                                                   HMI_RESYNC_FIELD_COUNT,
                                                   INTERVAL));
}

// Slot sayısı 13 (11 sayısal/metin + chg.val + far.pic durum göstergesi) ve
// far.pic FIELD_COUNT'tan hemen önceki (SON) alandır — enum sırası
// updateScreen gönderim sırasıyla birebir aynı olmalı (far.pic en sonda
// gönderilir). "headlight last" iddiası, yeni alan eklendiğinde de KORUNUR:
// yeni slotlar HEADLIGHT'ın ÖNÜNE eklenmelidir.
// NOT: 13 slotun 1'i (HMI_RESYNC_MOTOR_ERR) ATIL — gönderimi yoruma alındı
// (MOTOR_DRIVER_PRESENT=0, bkz. DisplayHMI.cpp); fiilen yenilenen alan sayısı
// 12'dir. Slot, enum↔gönderim sırası eşleşmesini kaydırmamak için duruyor ve
// tam tur süresini (13 × 500 ms = 6.5 sn) değiştirmiyor.
void test_resync_field_count_is_thirteen_headlight_last(void) {
    TEST_ASSERT_EQUAL_INT(13, (int)HMI_RESYNC_FIELD_COUNT);
    TEST_ASSERT_EQUAL_INT((int)HMI_RESYNC_FIELD_COUNT - 1,
                          (int)HMI_RESYNC_HEADLIGHT);
    // chg, contactor ile headlight ARASINDA olmalı (updateScreen sırası).
    TEST_ASSERT_EQUAL_INT((int)HMI_RESYNC_CONTACTOR + 1, (int)HMI_RESYNC_CHG);
    TEST_ASSERT_EQUAL_INT((int)HMI_RESYNC_CHG + 1, (int)HMI_RESYNC_HEADLIGHT);
}

// Ardışık vadelerde alanlar updateScreen sırasıyla (0..12) döner, sonra
// başa sarar.
void test_resync_round_robin_order_and_wrap(void) {
    uint32_t last = 0;
    uint8_t next = 0;
    for (int expected = 0; expected < HMI_RESYNC_FIELD_COUNT; ++expected) {
        const uint32_t now = (uint32_t)(expected + 1) * INTERVAL;
        TEST_ASSERT_EQUAL_INT(expected,
                              hmi_resync_due_field(now, last, next,
                                                   HMI_RESYNC_FIELD_COUNT,
                                                   INTERVAL));
    }
    // Tam turdan sonra başa dönmeli (speed).
    TEST_ASSERT_EQUAL_INT(
        HMI_RESYNC_SPEED,
        hmi_resync_due_field((uint32_t)(HMI_RESYNC_FIELD_COUNT + 1) * INTERVAL,
                             last, next, HMI_RESYNC_FIELD_COUNT, INTERVAL));
}

// ANA GARANTİ: 10 Hz görev simülasyonunda, tam tur süresi
// (HMI_RESYNC_FIELD_COUNT × INTERVAL) + 1 aralıklık marj içinde 13 slotun
// HER BİRİ en az bir kez zorla gönderilir — tespit edilemeyen bir Nextion
// reset'i sonrasında ekranın kendini toparlama üst sınırı budur (13 × 500 ms
// = 6.5 sn).
void test_resync_covers_all_fields_within_full_cycle(void) {
    uint32_t last = 0;
    uint8_t next = 0;
    bool seen[HMI_RESYNC_FIELD_COUNT] = {};

    const uint32_t horizon =
        (uint32_t)HMI_RESYNC_FIELD_COUNT * INTERVAL + INTERVAL;
    for (uint32_t now = 0; now <= horizon; now += TASK_PERIOD) {
        const int field = hmi_resync_due_field(now, last, next,
                                               HMI_RESYNC_FIELD_COUNT,
                                               INTERVAL);
        if (field >= 0) {
            TEST_ASSERT_TRUE(field < HMI_RESYNC_FIELD_COUNT);
            seen[field] = true;
        }
    }

    for (int i = 0; i < HMI_RESYNC_FIELD_COUNT; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(
            seen[i], "alan tam tur icinde hic zorla gonderilmedi");
    }
}

// Tick sayacı taşması (wraparound): unsigned çıkarma idiomu sayesinde vade
// hesabı UINT32_MAX sınırında da doğru çalışır.
void test_resync_tick_wraparound_safe(void) {
    uint32_t last = 0xFFFFFF00u;
    uint8_t next = 5;
    const uint32_t now = last + INTERVAL;  // bilerek taşar
    TEST_ASSERT_EQUAL_INT(5, hmi_resync_due_field(now, last, next,
                                                  HMI_RESYNC_FIELD_COUNT,
                                                  INTERVAL));
    TEST_ASSERT_EQUAL_UINT8(6, next);
}
