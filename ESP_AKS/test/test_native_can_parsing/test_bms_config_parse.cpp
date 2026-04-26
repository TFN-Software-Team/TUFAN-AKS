#include <unity.h>

#include "CanParse.h"

namespace {

twai_message_t makeBmsConfigMsg(uint8_t dlc,
                                uint8_t pack_hi, uint8_t pack_lo,
                                uint8_t cell_hi, uint8_t cell_lo) {
    twai_message_t m{};
    m.identifier = 0xE000;
    m.data_length_code = dlc;
    // data[0], data[1] kullanılmıyor; pack voltage data[2,3], cell mV data[4,5]
    m.data[2] = pack_hi;
    m.data[3] = pack_lo;
    m.data[4] = cell_hi;
    m.data[5] = cell_lo;
    return m;
}

}  // namespace

void test_bms_config_dlc_too_short(void) {
    twai_message_t m = makeBmsConfigMsg(5, 0x00, 0x50, 0x0F, 0xA0);
    TelemetryData out{};
    TEST_ASSERT_FALSE(CanParse::parseBmsConfig(m, out));
    TEST_ASSERT_FALSE(out.TEL_bmsDataValid);
    TEST_ASSERT_EQUAL_UINT16(0, out.TEL_bmsPackVoltageDeciV);
}

void test_bms_config_pack_voltage_big_endian(void) {
    // 0x0050 = 80 deci-V (8.0 V)
    twai_message_t m = makeBmsConfigMsg(6, 0x00, 0x50, 0x00, 0x00);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseBmsConfig(m, out));
    TEST_ASSERT_EQUAL_UINT16(80, out.TEL_bmsPackVoltageDeciV);
}

void test_bms_config_cell_voltage_big_endian(void) {
    // 0x0FA0 = 4000 mV
    twai_message_t m = makeBmsConfigMsg(6, 0x00, 0x00, 0x0F, 0xA0);
    TelemetryData out{};
    TEST_ASSERT_TRUE(CanParse::parseBmsConfig(m, out));
    TEST_ASSERT_EQUAL_UINT16(4000, out.TEL_bmsAverageCellVoltageMv);
}

void test_bms_config_sets_valid_flag(void) {
    twai_message_t m = makeBmsConfigMsg(6, 0x00, 0x00, 0x00, 0x00);
    TelemetryData out{};
    out.TEL_bmsDataValid = false;
    TEST_ASSERT_TRUE(CanParse::parseBmsConfig(m, out));
    TEST_ASSERT_TRUE(out.TEL_bmsDataValid);
}

void test_bms_config_preserves_other_fields(void) {
    twai_message_t m = makeBmsConfigMsg(6, 0x00, 0x55, 0x0F, 0xA0);
    TelemetryData out{};
    // Motor verisi ve BMS canlı verisi başlangıçta doluydu — config parse
    // bunları silmemeli.
    out.TEL_motorRpm = 1234;
    out.TEL_bmsSoc = 75;
    out.TEL_bmsCurrentDeciA = -42;
    out.TEL_bmsTemperatureC = 30;
    out.TEL_bmsErrorFlags = 0x10;

    CanParse::parseBmsConfig(m, out);

    TEST_ASSERT_EQUAL_UINT16(1234, out.TEL_motorRpm);
    TEST_ASSERT_EQUAL_UINT8(75, out.TEL_bmsSoc);
    TEST_ASSERT_EQUAL_INT16(-42, out.TEL_bmsCurrentDeciA);
    TEST_ASSERT_EQUAL_INT16(30, out.TEL_bmsTemperatureC);
    TEST_ASSERT_EQUAL_UINT8(0x10, out.TEL_bmsErrorFlags);
    // Yazılanlar değişmiş olmalı.
    TEST_ASSERT_EQUAL_UINT16(0x0055, out.TEL_bmsPackVoltageDeciV);
    TEST_ASSERT_EQUAL_UINT16(4000, out.TEL_bmsAverageCellVoltageMv);
}
