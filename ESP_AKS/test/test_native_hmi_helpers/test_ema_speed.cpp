#include <unity.h>
#include "HMIHelpers.h"

void test_ema_speed_normal_operation(void) {
    HmiEmaFilter filter(0.5f);
    
    // First update: 0.5 * 100 + 0.5 * 0 = 50
    float res1 = filter.update(100.0f, true);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, res1);

    // Second update: 0.5 * 100 + 0.5 * 50 = 75
    float res2 = filter.update(100.0f, true);
    TEST_ASSERT_EQUAL_FLOAT(75.0f, res2);
}

void test_ema_speed_stale_data_resets_filter(void) {
    HmiEmaFilter filter(0.5f);
    
    // Get to some value
    filter.update(100.0f, true);
    filter.update(100.0f, true);
    TEST_ASSERT_EQUAL_FLOAT(75.0f, filter.getSmoothed());

    // Invalid data (timeout)
    float res_invalid = filter.update(100.0f, false);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, res_invalid);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, filter.getSmoothed());

    // Recover from timeout - should start from 0, not 75
    // 0.5 * 100 + 0.5 * 0 = 50
    float res_recover = filter.update(100.0f, true);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, res_recover);
}
