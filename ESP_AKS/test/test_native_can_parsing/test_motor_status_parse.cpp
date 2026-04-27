#include <unity.h>

#include "CanParse.h"

namespace {

twai_message_t makeMotorMsg(uint8_t dlc, uint8_t b0, uint8_t b1, uint8_t b2,
                            uint8_t b3, uint8_t b4) {
    twai_message_t m{};
    m.identifier = 0x200;
    m.data_length_code = dlc;
    m.data[0] = b0;
    m.data[1] = b1;
    m.data[2] = b2;
    m.data[3] = b3;
    m.data[4] = b4;
    return m;
}

}  // namespace

void test_motor_status_dlc_too_short(void) {
    twai_message_t m = makeMotorMsg(3, 0xAA, 0xBB, 0xCC, 0xDD, 0x00);
    MotorStatus out{};
    TEST_ASSERT_FALSE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_FALSE(out.isValid);
    TEST_ASSERT_EQUAL_UINT16(0, out.rpm);
}

void test_motor_status_dlc_4_no_error_flag(void) {
    twai_message_t m = makeMotorMsg(4, 0x01, 0x90, 0x00, 0x32, 0xFF);
    MotorStatus out{};
    TEST_ASSERT_TRUE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_EQUAL_UINT16(0x0190, out.rpm);
    TEST_ASSERT_EQUAL_INT16(50, out.torqueFeedback);
    // DLC=4: errorFlags ignored (data[4] not part of valid payload).
    TEST_ASSERT_EQUAL_UINT8(0, out.errorFlags);
    TEST_ASSERT_TRUE(out.isValid);
}

void test_motor_status_dlc_5_with_error_flag(void) {
    twai_message_t m = makeMotorMsg(5, 0x00, 0x00, 0x00, 0x00, 0x42);
    MotorStatus out{};
    TEST_ASSERT_TRUE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_EQUAL_UINT8(0x42, out.errorFlags);
}

void test_motor_status_dlc_8_ok(void) {
    twai_message_t m = makeMotorMsg(8, 0x00, 0x10, 0x00, 0x05, 0xAA);
    MotorStatus out{};
    TEST_ASSERT_TRUE(CanParse::parseMotorStatus(m, out));
    TEST_ASSERT_EQUAL_UINT16(0x0010, out.rpm);
    TEST_ASSERT_EQUAL_UINT8(0xAA, out.errorFlags);
}

void test_motor_status_rpm_big_endian(void) {
    twai_message_t m = makeMotorMsg(4, 0x12, 0x34, 0x00, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_UINT16(0x1234, out.rpm);
}

void test_motor_status_rpm_zero(void) {
    twai_message_t m = makeMotorMsg(4, 0x00, 0x00, 0x00, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_UINT16(0, out.rpm);
}

void test_motor_status_rpm_max(void) {
    twai_message_t m = makeMotorMsg(4, 0xFF, 0xFF, 0x00, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, out.rpm);
}

void test_motor_status_torque_negative(void) {
    // 0xFFFF (uint) → -1 (int16)
    twai_message_t m = makeMotorMsg(4, 0x00, 0x00, 0xFF, 0xFF, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_INT16(-1, out.torqueFeedback);
}

void test_motor_status_torque_positive(void) {
    // 0x0100 → 256
    twai_message_t m = makeMotorMsg(4, 0x00, 0x00, 0x01, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_INT16(256, out.torqueFeedback);
}

void test_motor_status_torque_min_int16(void) {
    // 0x8000 → INT16_MIN (-32768)
    twai_message_t m = makeMotorMsg(4, 0x00, 0x00, 0x80, 0x00, 0x00);
    MotorStatus out{};
    CanParse::parseMotorStatus(m, out);
    TEST_ASSERT_EQUAL_INT16(-32768, out.torqueFeedback);
}

void test_motor_status_invalid_does_not_modify_out(void) {
    // DLC kısa: out olduğu gibi kalmalı (sentinel değerleri korunmalı).
    twai_message_t m = makeMotorMsg(2, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE);
    MotorStatus out{};
    out.rpm = 0xCAFE;
    out.torqueFeedback = -7;
    out.errorFlags = 0x55;
    out.isValid = false;

    TEST_ASSERT_FALSE(CanParse::parseMotorStatus(m, out));

    TEST_ASSERT_EQUAL_UINT16(0xCAFE, out.rpm);
    TEST_ASSERT_EQUAL_INT16(-7, out.torqueFeedback);
    TEST_ASSERT_EQUAL_UINT8(0x55, out.errorFlags);
    TEST_ASSERT_FALSE(out.isValid);
}
