#include <unity.h>
#include "MotorTimeoutDebounce.h"

void test_motor_timeout_debounce_normal(void) {
    uint8_t count = 0;
    uint32_t lastCheck = 0;
    uint32_t lastStatus = 0;
    const uint32_t TIMEOUT = 500;
    
    // Frame arrives at t=0
    lastStatus = 0;
    
    // t=500, check 1
    bool to = motor_timeout_debounce_evaluate(count, 500, lastCheck, lastStatus, TIMEOUT, 2);
    TEST_ASSERT_FALSE(to);
    TEST_ASSERT_EQUAL(1, count);
    
    // t=1000, check 2 -> should timeout!
    to = motor_timeout_debounce_evaluate(count, 1000, lastCheck, lastStatus, TIMEOUT, 2);
    TEST_ASSERT_TRUE(to);
    TEST_ASSERT_EQUAL(2, count);
}

void test_motor_timeout_debounce_recovered(void) {
    uint8_t count = 0;
    uint32_t lastCheck = 0;
    uint32_t lastStatus = 0;
    const uint32_t TIMEOUT = 500;
    
    // Frame arrives at t=0
    lastStatus = 0;
    
    // t=500, check 1
    bool to = motor_timeout_debounce_evaluate(count, 500, lastCheck, lastStatus, TIMEOUT, 2);
    TEST_ASSERT_FALSE(to);
    TEST_ASSERT_EQUAL(1, count);
    
    // Frame arrives at t=800
    lastStatus = 800;
    
    // t=1000, check 2 -> should reset counter!
    to = motor_timeout_debounce_evaluate(count, 1000, lastCheck, lastStatus, TIMEOUT, 2);
    TEST_ASSERT_FALSE(to);
    TEST_ASSERT_EQUAL(0, count);
}

void test_motor_timeout_debounce_rate_limiting(void) {
    uint8_t count = 0;
    uint32_t lastCheck = 0;
    uint32_t lastStatus = 0;
    const uint32_t TIMEOUT = 500;
    
    // t=499, should not evaluate
    bool to = motor_timeout_debounce_evaluate(count, 499, lastCheck, lastStatus, TIMEOUT, 2);
    TEST_ASSERT_FALSE(to);
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_EQUAL(0, lastCheck); // still 0
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_motor_timeout_debounce_normal);
    RUN_TEST(test_motor_timeout_debounce_recovered);
    RUN_TEST(test_motor_timeout_debounce_rate_limiting);
    return UNITY_END();
}
