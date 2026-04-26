#include <unity.h>

#include "CanParse.h"

namespace {

twai_message_t makeBmsLiveMsg(uint8_t dlc, uint8_t errorFlags, uint8_t current,
                              uint8_t reserved, uint8_t tempByte, uint8_t soc) {
    twai_message_t m{};
    m.identifier = 0xE001;
    m.data_length_code = dlc;
    m.data[0] = errorFlags;
    m.data[1] = current;
    m.data[2] = 0;  // unused in current parser
    m.data[3] = tempByte;
    m.data[4] = reserved;
    m.data[5] = soc;
    return m;
}

}  // namespace

void test_bms_live_dlc_too_short(void) {
    twai_message_t m = makeBmsLiveMsg(5, 0x10, 0x80, 0x00, 100, 50);
    TelemetryData out{};
    TEST_ASSERT_FALSE(CanParse::parseBmsLive(m, out));
    TEST_ASSERT_FALSE(out.TEL_bmsDataValid);
}

void test_bms_live_error_flags(void) {
    twai_message_t m = makeBmsLiveMsg(6, 0xC3, 0x00, 0x00, 100, 0);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseBmsLive(m, out));
    TEST_ASSERT_EQUAL_UINT8(0xC3, out.TEL_bmsErrorFlags);
}

void test_bms_live_current_signed_minus_one(void) {
    // 0xFF (uint8) cast to int8_t → -1
    twai_message_t m = makeBmsLiveMsg(6, 0, 0xFF, 0, 100, 0);
    TelemetryData out{};
    CanParse::parseBmsLive(m, out);
    TEST_ASSERT_EQUAL_INT16(-1, out.TEL_bmsCurrentDeciA);
}

void test_bms_live_current_signed_min(void) {
    // 0x80 → -128
    twai_message_t m = makeBmsLiveMsg(6, 0, 0x80, 0, 100, 0);
    TelemetryData out{};
    CanParse::parseBmsLive(m, out);
    TEST_ASSERT_EQUAL_INT16(-128, out.TEL_bmsCurrentDeciA);
}

void test_bms_live_current_signed_max(void) {
    // 0x7F → +127
    twai_message_t m = makeBmsLiveMsg(6, 0, 0x7F, 0, 100, 0);
    TelemetryData out{};
    CanParse::parseBmsLive(m, out);
    TEST_ASSERT_EQUAL_INT16(127, out.TEL_bmsCurrentDeciA);
}

void test_bms_live_current_zero(void) {
    twai_message_t m = makeBmsLiveMsg(6, 0, 0x00, 0, 100, 0);
    TelemetryData out{};
    CanParse::parseBmsLive(m, out);
    TEST_ASSERT_EQUAL_INT16(0, out.TEL_bmsCurrentDeciA);
}

void test_bms_live_temperature_zero(void) {
    // data[3]=100 → 100-100 = 0°C
    twai_message_t m = makeBmsLiveMsg(6, 0, 0, 0, 100, 0);
    TelemetryData out{};
    CanParse::parseBmsLive(m, out);
    TEST_ASSERT_EQUAL_INT16(0, out.TEL_bmsTemperatureC);
}

void test_bms_live_temperature_positive(void) {
    // data[3]=170 → 70°C
    twai_message_t m = makeBmsLiveMsg(6, 0, 0, 0, 170, 0);
    TelemetryData out{};
    CanParse::parseBmsLive(m, out);
    TEST_ASSERT_EQUAL_INT16(70, out.TEL_bmsTemperatureC);
}

void test_bms_live_temperature_negative(void) {
    // data[3]=80 → -20°C
    twai_message_t m = makeBmsLiveMsg(6, 0, 0, 0, 80, 0);
    TelemetryData out{};
    CanParse::parseBmsLive(m, out);
    TEST_ASSERT_EQUAL_INT16(-20, out.TEL_bmsTemperatureC);
}

void test_bms_live_soc(void) {
    twai_message_t m = makeBmsLiveMsg(6, 0, 0, 0, 100, 87);
    TelemetryData out{};
    CanParse::parseBmsLive(m, out);
    TEST_ASSERT_EQUAL_UINT8(87, out.TEL_bmsSoc);
}

void test_bms_live_sets_valid_flag(void) {
    twai_message_t m = makeBmsLiveMsg(6, 0, 0, 0, 100, 0);
    TelemetryData out{};
    out.TEL_bmsDataValid = false;
    CanParse::parseBmsLive(m, out);
    TEST_ASSERT_TRUE(out.TEL_bmsDataValid);
}
