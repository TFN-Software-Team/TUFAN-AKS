#include <unity.h>

#include <cstring>

#include "HMIHelpers.h"
#include "fake_uart.h"

namespace {
constexpr uint8_t END[3] = {0xFF, 0xFF, 0xFF};
}

// ---------------------------------------------------------------------------
// sendNumericIfChanged
// ---------------------------------------------------------------------------
void test_numeric_same_value_no_force_skips_write(void) {
    fake_uart_reset();
    HMI_sendNumericIfChanged("speed", 50, /*last*/ 50, /*force*/ false);
    TEST_ASSERT_EQUAL_size_t(0, fake_uart_get_size());
}

void test_numeric_changed_value_writes_command(void) {
    fake_uart_reset();
    HMI_sendNumericIfChanged("speed", 75, /*last*/ 50, /*force*/ false);

    // Beklenen: "speed.val=75" + 3 byte 0xFF terminatör
    const char* buf = fake_uart_get_buffer();
    size_t sz = fake_uart_get_size();
    TEST_ASSERT_NOT_NULL(strstr(buf, "speed.val=75"));
    TEST_ASSERT_TRUE(sz >= 3);
    TEST_ASSERT_EQUAL_UINT8(END[0], static_cast<uint8_t>(buf[sz - 3]));
    TEST_ASSERT_EQUAL_UINT8(END[1], static_cast<uint8_t>(buf[sz - 2]));
    TEST_ASSERT_EQUAL_UINT8(END[2], static_cast<uint8_t>(buf[sz - 1]));
}

void test_numeric_force_writes_even_when_unchanged(void) {
    fake_uart_reset();
    HMI_sendNumericIfChanged("speed", 50, /*last*/ 50, /*force*/ true);
    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), "speed.val=50"));
}

void test_numeric_negative_value_formatted(void) {
    fake_uart_reset();
    HMI_sendNumericIfChanged("torque", -123, /*last*/ 0, /*force*/ false);
    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), "torque.val=-123"));
}

// ---------------------------------------------------------------------------
// sendTextIfChanged
// ---------------------------------------------------------------------------
void test_text_same_value_no_force_skips_write(void) {
    fake_uart_reset();
    HMI_sendTextIfChanged("state", "DRIVE", /*last*/ "DRIVE", /*force*/ false);
    TEST_ASSERT_EQUAL_size_t(0, fake_uart_get_size());
}

void test_text_changed_value_writes_command(void) {
    fake_uart_reset();
    HMI_sendTextIfChanged("state", "DRIVE", /*last*/ "READY", /*force*/ false);
    TEST_ASSERT_NOT_NULL(
        strstr(fake_uart_get_buffer(), "state.txt=\"DRIVE\""));
}

void test_text_force_writes_even_when_unchanged(void) {
    fake_uart_reset();
    HMI_sendTextIfChanged("state", "IDLE", /*last*/ "IDLE", /*force*/ true);
    TEST_ASSERT_NOT_NULL(
        strstr(fake_uart_get_buffer(), "state.txt=\"IDLE\""));
}

void test_text_terminated_with_end_bytes(void) {
    fake_uart_reset();
    HMI_sendTextIfChanged("err", "0x42", /*last*/ "0x00", /*force*/ false);

    size_t sz = fake_uart_get_size();
    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_TRUE(sz >= 3);
    TEST_ASSERT_EQUAL_UINT8(END[0], static_cast<uint8_t>(buf[sz - 3]));
    TEST_ASSERT_EQUAL_UINT8(END[1], static_cast<uint8_t>(buf[sz - 2]));
    TEST_ASSERT_EQUAL_UINT8(END[2], static_cast<uint8_t>(buf[sz - 1]));
}

// ---------------------------------------------------------------------------
// sendEndBytes — tek başına 3 byte 0xFF yazmalı.
// ---------------------------------------------------------------------------
void test_sendEndBytes_writes_three_ff(void) {
    fake_uart_reset();
    HMI_sendEndBytes();

    TEST_ASSERT_EQUAL_size_t(3, fake_uart_get_size());
    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_EQUAL_UINT8(0xFF, static_cast<uint8_t>(buf[0]));
    TEST_ASSERT_EQUAL_UINT8(0xFF, static_cast<uint8_t>(buf[1]));
    TEST_ASSERT_EQUAL_UINT8(0xFF, static_cast<uint8_t>(buf[2]));
}
