#include <unity.h>
#include <cstring>

#include "HMIHelpers.h"

void test_state_text_init(void) {
    TEST_ASSERT_EQUAL_STRING("INIT", HMI_getStateText(HMI_VcuState::INIT));
}

void test_state_text_idle(void) {
    TEST_ASSERT_EQUAL_STRING("IDLE", HMI_getStateText(HMI_VcuState::IDLE));
}

void test_state_text_ready(void) {
    TEST_ASSERT_EQUAL_STRING("READY", HMI_getStateText(HMI_VcuState::READY));
}

void test_state_text_drive(void) {
    TEST_ASSERT_EQUAL_STRING("DRIVE", HMI_getStateText(HMI_VcuState::DRIVE));
}

void test_state_text_emergency_stop(void) {
    TEST_ASSERT_EQUAL_STRING("ESTOP",
                             HMI_getStateText(HMI_VcuState::EMERGENCY_STOP));
}

void test_state_text_fault(void) {
    TEST_ASSERT_EQUAL_STRING("FAULT", HMI_getStateText(HMI_VcuState::FAULT));
}

void test_state_text_unknown_falls_back(void) {
    // Switch fallthrough: enum dışı bir değer cast edilirse "UNK" dönmeli.
    HMI_VcuState invalid = static_cast<HMI_VcuState>(99);
    TEST_ASSERT_EQUAL_STRING("UNK", HMI_getStateText(invalid));
}
