// SysStateDerive (Y33 kararı, bkz. SysStateDerive.h) — SYSSTATE_DERIVE_FROM_
// CURRENT=1 derlemesi: türetme kuralları, ezmeme kuralı ve bayat-veri koruması.
//
// Bayrak Y33 sonrası ZATEN varsayılan olarak 1'dir; buradaki açık #define,
// bu suite'in üretim varsayılanından BAĞIMSIZ olarak flag=1 dalını
// doğrulamasını garanti eder (aynı desen: test_native_ready_motor/test_main.cpp,
// MOTOR_DRIVER_PRESENT=1).
#define SYSSTATE_DERIVE_FROM_CURRENT 1

#include <unity.h>

#include "SysStateDerive.h"

// Bayrağın gerçekten 1 olduğunu (SystemConfig.h #ifndef guard'ı ezmediğinden)
// doğrula — aksi halde aşağıdaki testler yanıltıcı şekilde geçerdi.
static_assert(SYSSTATE_DERIVE_FROM_CURRENT == 1,
              "Bu binary SYSSTATE_DERIVE_FROM_CURRENT=1 ile derlenmeli");

// Y20 saha gozlemi: sarjda +9.8 A (pozitif) -> Charge(3).
void test_apply_derives_charge_when_sysstate_zero_and_current_above_band(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;
    d.TEL_bmsDataValid = true;
    d.TEL_bmsCurrentCentiA = 980;  // 9.8 A sarj (Y20 olcumu)

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(3, d.TEL_bmsSystemState);
}

// Y20 saha gozlemi: suruste -5.6 A (negatif) -> Discharge(1).
void test_apply_derives_discharge_when_sysstate_zero_and_current_below_band(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;
    d.TEL_bmsDataValid = true;
    d.TEL_bmsCurrentCentiA = -560;  // -5.6 A surus (Y20 olcumu)

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(1, d.TEL_bmsSystemState);
}

// Y20 saha gozlemi: bosta -0.1 A -> bant icinde kalir -> IDLE(2).
void test_apply_derives_idle_when_sysstate_zero_and_current_within_band(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;
    d.TEL_bmsDataValid = true;
    d.TEL_bmsCurrentCentiA = -10;  // -0.1 A — bosta gozlemlenen tipik deger

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(2, d.TEL_bmsSystemState);
}

// BAYAT VERI KORUMASI: BMS verisi taze DEGILKEN akim son gorulen degeri
// tutar. O bayat akimdan "sarj ediliyor" sonucu cikarmak YANILTICI olurdu;
// notr 2 (BOSTA) yazilir. Gercek bilgi ayni frame'deki bmsValid=0 alanindadir.
void test_apply_returns_neutral_when_bms_data_stale(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;
    d.TEL_bmsDataValid = false;    // BMS verisi taze DEGIL
    d.TEL_bmsCurrentCentiA = 980;  // bayat "sarj" akimi — GUVENILMEZ

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(2, d.TEL_bmsSystemState);  // Charge(3) DEGIL
}

// flag=1 AMA TEL_bmsSystemState ZATEN 0'dan farkli (gercek parse eklenmis
// gibi davranan bir deger) -> EZILMEMELI, dokunulmadan kalmali.
void test_apply_does_not_overwrite_when_sysstate_already_nonzero(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 2;         // "zaten gercek parse ile dolduruldu" varsayimi
    d.TEL_bmsDataValid = true;
    d.TEL_bmsCurrentCentiA = 990;     // derive edilseydi Charge(3) cikardi

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(2, d.TEL_bmsSystemState);  // DEGISMEDI
}

// FAULT(4) da "zaten dolu" sayilir — derive FAULT uretmedigi icin bu deger
// asla derive ciktisiyla karisamaz, ama yine de EZILMEMELI kuralini kanitlar.
// (Ileride gercek bir BMS-durum CAN ID'si bulunursa bu yol onu korur.)
void test_apply_does_not_overwrite_existing_fault_state(void) {
    TelemetryData d = {};
    d.TEL_bmsSystemState = 4;
    d.TEL_bmsDataValid = true;
    d.TEL_bmsCurrentCentiA = 0;

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(4, d.TEL_bmsSystemState);
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_derives_charge_when_sysstate_zero_and_current_above_band);
    RUN_TEST(test_apply_derives_discharge_when_sysstate_zero_and_current_below_band);
    RUN_TEST(test_apply_derives_idle_when_sysstate_zero_and_current_within_band);
    RUN_TEST(test_apply_returns_neutral_when_bms_data_stale);
    RUN_TEST(test_apply_does_not_overwrite_when_sysstate_already_nonzero);
    RUN_TEST(test_apply_does_not_overwrite_existing_fault_state);
    return UNITY_END();
}
