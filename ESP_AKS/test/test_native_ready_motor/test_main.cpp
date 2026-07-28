// Motor kaynakli interlock'lar — MOTOR_DRIVER_PRESENT=1 derlemesi.
//
// isReadyEntryPermitted(), hasCriticalCondition() ve isResetInterlockSatisfied()
// saf/inline header predicate'leri oldugundan, MOTOR_DRIVER_PRESENT=1 dalinin
// davranisi yalnizca bayragi 1 olarak goren bir derleme biriminde
// dogrulanabilir. Native test binary'si VcuLogic.cpp'yi (dolayisiyla run()
// durum makinesini) bayrak 0 ile derler; bu yuzden motor sartlarini ayri bir
// binary'de, dogrudan predicate uzerinden test ediyoruz (predicate saf: run()'a
// / global state'e ihtiyac yok, link gerekmez).
//
// KAPSAM NOTU: bayrak 0 iken motor kaynakli TUM kontroller devre disidir
// (0x200'u bugun hall-effect hiz sensoru uretiyor — bkz. VcuLogic.h'deki
// #if MOTOR_DRIVER_PRESENT bloklari ve Documents/MOTOR_ENTEGRASYON_NOTU.md §6).
// Bayrak 0 davranisi test_native_vcu_logic'te, bayrak 1 davranisi BURADA
// kilitlenir — motor surucusu entegre edilince geri gelmesi gereken sartlarin
// tamami bu dosyada yazilidir.
#define MOTOR_DRIVER_PRESENT 1

#include <unity.h>

#include "VcuLogic.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;
using VcuLogic::VcuState;

// Bayragin gercekten 1 oldugunu (SystemConfig.h #ifndef guard'i ezmediginden)
// dogrula — aksi halde asagidaki testler yaniltici sekilde gecerdi.
static_assert(MOTOR_DRIVER_PRESENT == 1,
              "Bu binary MOTOR_DRIVER_PRESENT=1 ile derlenmeli");

// MOTOR_DRIVER_PRESENT=1 iken: bmsDataValid=true + kritik/uyari yok olsa bile
// motorDataValid=false ise READY girisi REDDEDILIR.
void test_ready_entry_rejected_when_motor_data_invalid(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorDataValid = false;

    TEST_ASSERT_FALSE(VcuLogic::isReadyEntryPermitted(d));
}

// MOTOR_DRIVER_PRESENT=1 iken taze motor verisi varsa (ve gerisi temizse)
// READY girisine izin verilir.
void test_ready_entry_permitted_when_motor_data_valid(void) {
    TelemetryData d = makeTelemetryDataValid();  // motorDataValid=true

    TEST_ASSERT_TRUE(VcuLogic::isReadyEntryPermitted(d));
}

// ---------------------------------------------------------------------------
// hasCriticalCondition — motor kaynakli girdiler bayrak 1 iken GERI GELIR.
// ---------------------------------------------------------------------------
// Motor error flag'i: bayrak 1 iken 0x200'un kaynagi gercekten motor
// surucusudur, dolayisiyla data[7] hata bitleri yeniden karar girdisidir.
void test_motor_error_flag_is_critical_when_flag1(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorErrorFlags = 0x01;
    TEST_ASSERT_TRUE(VcuLogic::hasCriticalCondition(d, VcuState::READY));
}

// Motor freshness kaybi: IDLE'da tolere edilir, READY/DRIVE'da kritik.
void test_motor_timeout_in_idle_is_safe_when_flag1(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_FALSE(VcuLogic::hasCriticalCondition(d, VcuState::IDLE));
}

void test_motor_timeout_in_ready_is_critical_when_flag1(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_TRUE(VcuLogic::hasCriticalCondition(d, VcuState::READY));
}

void test_motor_timeout_in_drive_is_critical_when_flag1(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_TRUE(VcuLogic::hasCriticalCondition(d, VcuState::DRIVE));
}

// ---------------------------------------------------------------------------
// isResetInterlockSatisfied — motor kaynakli sartlar bayrak 1 iken GERI GELIR.
// ---------------------------------------------------------------------------
void test_reset_interlock_motor_error_blocks_when_flag1(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorErrorFlags = 0x02;
    TEST_ASSERT_FALSE(VcuLogic::isResetInterlockSatisfied(d, VcuState::FAULT));
}

// AKS-04 fail-safe: bayat motor verisiyle FAULT/E-STOP'tan cikilmaz.
void test_reset_interlock_motor_stream_lost_blocks_when_flag1(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorDataValid = false;
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_FALSE(VcuLogic::isResetInterlockSatisfied(d, VcuState::FAULT));
    TEST_ASSERT_FALSE(
        VcuLogic::isResetInterlockSatisfied(d, VcuState::EMERGENCY_STOP));
}

// Hareket halinde reset yasagi (VCU_RESET_MAX_RPM) bayraktan BAGIMSIZDIR —
// bayrak 1 iken de taze veriyle aynen calisir.
void test_reset_interlock_high_rpm_blocks_when_flag1(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorRpm = 500;
    TEST_ASSERT_FALSE(VcuLogic::isResetInterlockSatisfied(d, VcuState::FAULT));
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_ready_entry_rejected_when_motor_data_invalid);
    RUN_TEST(test_ready_entry_permitted_when_motor_data_valid);
    RUN_TEST(test_motor_error_flag_is_critical_when_flag1);
    RUN_TEST(test_motor_timeout_in_idle_is_safe_when_flag1);
    RUN_TEST(test_motor_timeout_in_ready_is_critical_when_flag1);
    RUN_TEST(test_motor_timeout_in_drive_is_critical_when_flag1);
    RUN_TEST(test_reset_interlock_motor_error_blocks_when_flag1);
    RUN_TEST(test_reset_interlock_motor_stream_lost_blocks_when_flag1);
    RUN_TEST(test_reset_interlock_high_rpm_blocks_when_flag1);
    return UNITY_END();
}
