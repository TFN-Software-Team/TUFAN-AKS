#include <unity.h>

// ---------------------------------------------------------------------------
// Test fonksiyonlarının ileri bildirimleri.
// Tanımlar test_safety_thresholds.cpp ve test_reset_interlock.cpp içindedir.
// ---------------------------------------------------------------------------
// Faz 1 — isCurrentCritical
extern void test_isCurrentCritical_charge_below_threshold(void);
extern void test_isCurrentCritical_charge_at_threshold(void);
extern void test_isCurrentCritical_charge_above_threshold(void);
extern void test_isCurrentCritical_zero_is_safe(void);
extern void test_isCurrentCritical_discharge_below_threshold(void);
extern void test_isCurrentCritical_discharge_at_threshold(void);
extern void test_isCurrentCritical_discharge_above_threshold(void);

// Faz 1 — isCurrentWarning
extern void test_isCurrentWarning_charge_below_threshold(void);
extern void test_isCurrentWarning_charge_at_threshold(void);
extern void test_isCurrentWarning_discharge_below_threshold(void);
extern void test_isCurrentWarning_discharge_at_threshold(void);

// Faz 1 — sıcaklık eşikleri
extern void test_warning_temp_below_threshold(void);
extern void test_warning_temp_at_warn_threshold(void);
extern void test_critical_temp_at_critical_threshold(void);

// Faz 1 — voltaj eşikleri
extern void test_warning_voltage_above_warn_low(void);
extern void test_warning_voltage_at_warn_low(void);
extern void test_critical_voltage_at_crit_low(void);
extern void test_warning_voltage_below_warn_high(void);
extern void test_warning_voltage_at_warn_high(void);
extern void test_critical_voltage_at_crit_high(void);

// Faz 1 — error flag'ler
extern void test_critical_motor_error_flag_set(void);
extern void test_critical_bms_error_flag_set(void);

// Faz 1 — motor timeout
extern void test_motor_timeout_in_idle_is_safe(void);
extern void test_motor_timeout_in_ready_is_critical(void);
extern void test_motor_timeout_in_drive_is_critical(void);

// Faz 1 — bms data invalid
extern void test_warning_bms_invalid_skips_thresholds(void);
extern void test_critical_bms_invalid_with_motor_error_still_critical(void);

// Faz 1 — baseline & akım uçtan uca
extern void test_baseline_clean_data_no_conditions(void);
extern void test_critical_via_charge_current(void);
extern void test_critical_via_discharge_current(void);

// Faz 1 — reset interlock
extern void test_reset_interlock_clean_baseline_passes(void);
extern void test_reset_interlock_motor_error_blocks(void);
extern void test_reset_interlock_bms_error_blocks(void);
extern void test_reset_interlock_critical_temperature_blocks(void);
extern void test_reset_interlock_critical_voltage_low_blocks(void);
extern void test_reset_interlock_critical_voltage_high_blocks(void);
extern void test_reset_interlock_critical_current_blocks(void);
extern void test_reset_interlock_motor_timeout_in_fault_blocks(void);
extern void test_reset_interlock_warning_level_passes(void);

// Faz 0 sanity
static void test_smoke_arithmetic(void) {
    TEST_ASSERT_EQUAL_INT(2, 1 + 1);
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    // Faz 0
    RUN_TEST(test_smoke_arithmetic);

    // Faz 1 — akım eşikleri (saf fonksiyon)
    RUN_TEST(test_isCurrentCritical_charge_below_threshold);
    RUN_TEST(test_isCurrentCritical_charge_at_threshold);
    RUN_TEST(test_isCurrentCritical_charge_above_threshold);
    RUN_TEST(test_isCurrentCritical_zero_is_safe);
    RUN_TEST(test_isCurrentCritical_discharge_below_threshold);
    RUN_TEST(test_isCurrentCritical_discharge_at_threshold);
    RUN_TEST(test_isCurrentCritical_discharge_above_threshold);
    RUN_TEST(test_isCurrentWarning_charge_below_threshold);
    RUN_TEST(test_isCurrentWarning_charge_at_threshold);
    RUN_TEST(test_isCurrentWarning_discharge_below_threshold);
    RUN_TEST(test_isCurrentWarning_discharge_at_threshold);

    // Faz 1 — sıcaklık
    RUN_TEST(test_warning_temp_below_threshold);
    RUN_TEST(test_warning_temp_at_warn_threshold);
    RUN_TEST(test_critical_temp_at_critical_threshold);

    // Faz 1 — pack voltajı
    RUN_TEST(test_warning_voltage_above_warn_low);
    RUN_TEST(test_warning_voltage_at_warn_low);
    RUN_TEST(test_critical_voltage_at_crit_low);
    RUN_TEST(test_warning_voltage_below_warn_high);
    RUN_TEST(test_warning_voltage_at_warn_high);
    RUN_TEST(test_critical_voltage_at_crit_high);

    // Faz 1 — error flag'ler
    RUN_TEST(test_critical_motor_error_flag_set);
    RUN_TEST(test_critical_bms_error_flag_set);

    // Faz 1 — motor timeout
    RUN_TEST(test_motor_timeout_in_idle_is_safe);
    RUN_TEST(test_motor_timeout_in_ready_is_critical);
    RUN_TEST(test_motor_timeout_in_drive_is_critical);

    // Faz 1 — bms invalid
    RUN_TEST(test_warning_bms_invalid_skips_thresholds);
    RUN_TEST(test_critical_bms_invalid_with_motor_error_still_critical);

    // Faz 1 — baseline & akım entegre
    RUN_TEST(test_baseline_clean_data_no_conditions);
    RUN_TEST(test_critical_via_charge_current);
    RUN_TEST(test_critical_via_discharge_current);

    // Faz 1 — reset interlock
    RUN_TEST(test_reset_interlock_clean_baseline_passes);
    RUN_TEST(test_reset_interlock_motor_error_blocks);
    RUN_TEST(test_reset_interlock_bms_error_blocks);
    RUN_TEST(test_reset_interlock_critical_temperature_blocks);
    RUN_TEST(test_reset_interlock_critical_voltage_low_blocks);
    RUN_TEST(test_reset_interlock_critical_voltage_high_blocks);
    RUN_TEST(test_reset_interlock_critical_current_blocks);
    RUN_TEST(test_reset_interlock_motor_timeout_in_fault_blocks);
    RUN_TEST(test_reset_interlock_warning_level_passes);

    return UNITY_END();
}
