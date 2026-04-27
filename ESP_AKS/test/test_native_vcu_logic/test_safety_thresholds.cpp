#include <unity.h>

#include "VcuLogic.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;
using VcuLogic::hasCriticalCondition;
using VcuLogic::hasWarningCondition;
using VcuLogic::isCurrentCritical;
using VcuLogic::isCurrentWarning;
using VcuLogic::VcuState;

// ---------------------------------------------------------------------------
// isCurrentCritical — charge tarafı (eşik: BMS_CRITICAL_MAX_CHARGE_CURRENT_DECI_A = 10)
// ---------------------------------------------------------------------------
void test_isCurrentCritical_charge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentCritical(9));
}

void test_isCurrentCritical_charge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(10));
}

void test_isCurrentCritical_charge_above_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(11));
}

void test_isCurrentCritical_zero_is_safe(void) {
    TEST_ASSERT_FALSE(isCurrentCritical(0));
}

// ---------------------------------------------------------------------------
// isCurrentCritical — discharge tarafı (eşik: -150)
// ---------------------------------------------------------------------------
void test_isCurrentCritical_discharge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentCritical(-149));
}

void test_isCurrentCritical_discharge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(-150));
}

void test_isCurrentCritical_discharge_above_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentCritical(-200));
}

// ---------------------------------------------------------------------------
// isCurrentWarning — charge / discharge (eşikler: 9 / -90)
// ---------------------------------------------------------------------------
void test_isCurrentWarning_charge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentWarning(8));
}

void test_isCurrentWarning_charge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentWarning(9));
}

void test_isCurrentWarning_discharge_below_threshold(void) {
    TEST_ASSERT_FALSE(isCurrentWarning(-89));
}

void test_isCurrentWarning_discharge_at_threshold(void) {
    TEST_ASSERT_TRUE(isCurrentWarning(-90));
}

// ---------------------------------------------------------------------------
// hasWarningCondition / hasCriticalCondition — sıcaklık (warn 55, crit 70)
// ---------------------------------------------------------------------------
void test_warning_temp_below_threshold(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTemperatureC = 54;
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_warning_temp_at_warn_threshold(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTemperatureC = 55;
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_temp_at_critical_threshold(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTemperatureC = 70;
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Pack voltajı alt sınır (warn ≤74, crit ≤70 deci-V)
// ---------------------------------------------------------------------------
void test_warning_voltage_above_warn_low(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 75;
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_warning_voltage_at_warn_low(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 74;
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_voltage_at_crit_low(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 70;
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Pack voltajı üst sınır (warn ≥85, crit ≥87)
// ---------------------------------------------------------------------------
void test_warning_voltage_below_warn_high(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 84;
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_warning_voltage_at_warn_high(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 85;
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_voltage_at_crit_high(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 87;
    TEST_ASSERT_TRUE(hasWarningCondition(d));
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Error flag'leri her zaman critical tetikler (data valid olmasa bile).
// ---------------------------------------------------------------------------
void test_critical_motor_error_flag_set(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorErrorFlags = 0x01;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_bms_error_flag_set(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsErrorFlags = 0x80;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Motor timeout — IDLE'de yok sayılır, diğer durumlarda critical.
// ---------------------------------------------------------------------------
void test_motor_timeout_in_idle_is_safe(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::IDLE));
}

void test_motor_timeout_in_ready_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

void test_motor_timeout_in_drive_is_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::DRIVE));
}

// ---------------------------------------------------------------------------
// BMS data invalid — eşik kontrolü yapılmaz, ama motor/bms error hâlâ critical.
// ---------------------------------------------------------------------------
void test_warning_bms_invalid_skips_thresholds(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsDataValid = false;
    d.TEL_bmsTemperatureC = 99;        // gerçekte critical olurdu
    d.TEL_bmsPackVoltageDeciV = 50;    // gerçekte critical olurdu
    d.TEL_bmsCurrentDeciA = -250;      // gerçekte critical olurdu
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_bms_invalid_with_motor_error_still_critical(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsDataValid = false;
    d.TEL_motorErrorFlags = 0x04;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

// ---------------------------------------------------------------------------
// Baseline — temiz veri hiçbir koşul tetiklemez.
// ---------------------------------------------------------------------------
void test_baseline_clean_data_no_conditions(void) {
    TelemetryData d = makeTelemetryDataValid();
    TEST_ASSERT_FALSE(hasWarningCondition(d));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::IDLE));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::READY));
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::DRIVE));
}

// ---------------------------------------------------------------------------
// Akım eşikleri uçtan uca BMS verisi içinden de doğrulansın.
// ---------------------------------------------------------------------------
void test_critical_via_charge_current(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentDeciA = 12;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}

void test_critical_via_discharge_current(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentDeciA = -160;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::READY));
}
