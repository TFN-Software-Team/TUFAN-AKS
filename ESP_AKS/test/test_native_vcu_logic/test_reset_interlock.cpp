#include <unity.h>

#include "VcuLogic.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;
using VcuLogic::isResetInterlockSatisfied;
using VcuLogic::VcuState;

// ---------------------------------------------------------------------------
// Reset interlock — FAULT/EMERGENCY_STOP'tan IDLE'ya geçiş için ön koşul.
// Hem motor/bms error flag temiz olmalı, hem de hasCriticalCondition false
// olmalı. Aksi halde reset reddedilir.
// ---------------------------------------------------------------------------
void test_reset_interlock_clean_baseline_passes(void) {
    TelemetryData d = makeTelemetryDataValid();
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::EMERGENCY_STOP));
}

void test_reset_interlock_motor_error_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorErrorFlags = 0x02;
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_bms_error_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsErrorFlags = 0x10;
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_critical_temperature_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTemperatureC = 75;  // > 70°C critical
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_critical_voltage_low_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 65;  // < 70 dV critical
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_critical_voltage_high_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 90;  // > 87 dV critical
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_critical_current_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentDeciA = -200;  // discharge critical
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// FAULT içerisinde motor timeout aktifse: state IDLE değil (FAULT), bu yüzden
// hasCriticalCondition true → reset reddedilir.
void test_reset_interlock_motor_timeout_in_fault_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// Sadece warning seviyesindeki bir koşul reset'i bloklamamalı.
void test_reset_interlock_warning_level_passes(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTemperatureC = 60;  // warning ama critical değil
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}
