#include <unity.h>
#include <cstring>

#include "HMIHelpers.h"

void test_error_format_zero(void) {
    char buf[16];
    HMI_formatErrorText(0x00, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0x00", buf);
}

void test_error_format_full_byte(void) {
    char buf[16];
    HMI_formatErrorText(0xFF, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0xFF", buf);
}

void test_error_format_specific_value(void) {
    char buf[16];
    HMI_formatErrorText(0x05, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0x05", buf);
}

void test_error_format_uppercase_hex(void) {
    // 0xAB hex'in büyük harf 'A' ve 'B' ile yazıldığı doğrulanır.
    char buf[16];
    HMI_formatErrorText(0xAB, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0xAB", buf);
}

void test_error_format_zero_buffer_size_no_crash(void) {
    char buf[16];
    std::memset(buf, '?', sizeof(buf));
    HMI_formatErrorText(0xFF, buf, 0);
    // Buffer dokunulmamış olmalı.
    TEST_ASSERT_EQUAL_CHAR('?', buf[0]);
}

void test_error_format_truncates_to_small_buffer(void) {
    // Buffer 1 byte: snprintf tek bir null byte yazar.
    char buf[1];
    buf[0] = 'X';
    HMI_formatErrorText(0xFF, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
}
