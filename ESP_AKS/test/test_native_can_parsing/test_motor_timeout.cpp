#include <unity.h>

#include "CanParse.h"

// CAN_MOTOR_STATUS_TIMEOUT_MS = 500. Native testte pdMS_TO_TICKS identity
// olduğundan timeoutTicks = 500 doğrudan kullanılır.
static constexpr TickType_t TIMEOUT = 500;

void test_timeout_not_seen_yet(void) {
    // hasSeen=false → her durumda false dönmeli.
    TEST_ASSERT_FALSE(
        CanParse::isMotorStatusTimedOut(false, true, 1000, 0, TIMEOUT));
    TEST_ASSERT_FALSE(
        CanParse::isMotorStatusTimedOut(false, false, 1000, 0, TIMEOUT));
}

void test_timeout_already_invalidated(void) {
    // lastValid=false → zaten timeout işaretli, tekrar true dönmemeli.
    TEST_ASSERT_FALSE(
        CanParse::isMotorStatusTimedOut(true, false, 10000, 0, TIMEOUT));
}

void test_timeout_within_window(void) {
    // 499 ms geçti, 500 ms eşiği henüz aşılmadı → false.
    TEST_ASSERT_FALSE(
        CanParse::isMotorStatusTimedOut(true, true, 499, 0, TIMEOUT));
}

void test_timeout_at_threshold(void) {
    // Eşitlik durumu: >= ile karşılaştırıldığı için true.
    TEST_ASSERT_TRUE(
        CanParse::isMotorStatusTimedOut(true, true, 500, 0, TIMEOUT));
}

void test_timeout_past_threshold(void) {
    TEST_ASSERT_TRUE(
        CanParse::isMotorStatusTimedOut(true, true, 1234, 0, TIMEOUT));
}

void test_timeout_just_received(void) {
    // now == lastTick → diff = 0 → false.
    TEST_ASSERT_FALSE(
        CanParse::isMotorStatusTimedOut(true, true, 12345, 12345, TIMEOUT));
}

void test_timeout_tick_wraparound_within_window(void) {
    // lastTick=0xFFFFFFFE, now=10 → unsigned diff = 12 → false.
    TickType_t lastTick = 0xFFFFFFFEu;
    TickType_t now = 10;
    TEST_ASSERT_FALSE(
        CanParse::isMotorStatusTimedOut(true, true, now, lastTick, TIMEOUT));
}

void test_timeout_tick_wraparound_at_threshold(void) {
    // lastTick=0xFFFFFFFE, now = 0xFFFFFFFE + 500 = 0x000001FC = 508
    // unsigned diff = 500 → true.
    TickType_t lastTick = 0xFFFFFFFEu;
    TickType_t now = static_cast<TickType_t>(lastTick + 500);
    TEST_ASSERT_TRUE(
        CanParse::isMotorStatusTimedOut(true, true, now, lastTick, TIMEOUT));
}
