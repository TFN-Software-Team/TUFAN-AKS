#include <unity.h>

#include "CanParse.h"
#include "SystemConfig.h"

// isBmsStatusTimedOut: isMotorStatusTimedOut ile birebir aynı mantık.
// Sabit: CAN_BMS_STATUS_TIMEOUT_MS = 60000000 (SystemConfig.h).
// Native testte pdMS_TO_TICKS identity olduğundan CAN_BMS_STATUS_TIMEOUT_MS doğrudan kullanılır.
static constexpr TickType_t BMS_TIMEOUT = CAN_BMS_STATUS_TIMEOUT_MS;

void test_bms_timeout_not_seen_yet(void) {
    // hasSeen=false → henüz mesaj görülmedi, timeout sayılmaz.
    TEST_ASSERT_FALSE(
        CanParse::isBmsStatusTimedOut(false, true, 1000, 0, BMS_TIMEOUT));
    TEST_ASSERT_FALSE(
        CanParse::isBmsStatusTimedOut(false, false, 1000, 0, BMS_TIMEOUT));
}

void test_bms_timeout_already_invalidated(void) {
    // lastValid=false → zaten invalidate edilmiş, tekrar tetiklenmemeli.
    TEST_ASSERT_FALSE(
        CanParse::isBmsStatusTimedOut(true, false, 10000, 0, BMS_TIMEOUT));
}

void test_bms_timeout_within_window(void) {
    // BMS_TIMEOUT-1 ms geçti, eşik aşılmadı → false.
    TEST_ASSERT_FALSE(
        CanParse::isBmsStatusTimedOut(true, true, BMS_TIMEOUT - 1, 0, BMS_TIMEOUT));
}

void test_bms_timeout_at_threshold(void) {
    // Tam BMS_TIMEOUT ms: >= karşılaştırması → true.
    TEST_ASSERT_TRUE(
        CanParse::isBmsStatusTimedOut(true, true, BMS_TIMEOUT, 0, BMS_TIMEOUT));
}

void test_bms_timeout_past_threshold(void) {
    // BMS_TIMEOUT'tan fazla → kesinlikle true.
    TEST_ASSERT_TRUE(
        CanParse::isBmsStatusTimedOut(true, true, BMS_TIMEOUT + 1000, 0, BMS_TIMEOUT));
}
