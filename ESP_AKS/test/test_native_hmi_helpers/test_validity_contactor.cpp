#include <unity.h>

#include "HMIHelpers.h"

void test_validity_valid_no_timeout(void) {
    TEST_ASSERT_EQUAL_STRING("VALID", HMI_getValidityText(true, false));
}

void test_validity_invalid_no_timeout(void) {
    TEST_ASSERT_EQUAL_STRING("INVALID", HMI_getValidityText(false, false));
}

void test_validity_timeout_overrides_invalid(void) {
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", HMI_getValidityText(false, true));
}

void test_validity_timeout_overrides_valid(void) {
    // Timeout her durumda öncelikli olmalı — valid=true bile olsa.
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", HMI_getValidityText(true, true));
}

void test_contactor_closed(void) {
    TEST_ASSERT_EQUAL_STRING("CLOSED", HMI_getContactorText(true));
}

void test_contactor_open(void) {
    TEST_ASSERT_EQUAL_STRING("OPEN", HMI_getContactorText(false));
}
