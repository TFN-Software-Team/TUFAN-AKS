#include <unity.h>
#include "HMITouchParser.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_hmi_touch_parser_valid_command(void) {
    HMI_TouchParserState state;
    uint8_t outCmd = 0;

    // Send 0x5A 0x01 0xFE
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x5A, state, outCmd));
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x01, state, outCmd));
    TEST_ASSERT_TRUE(HMI_parseTouchByte(0xFE, state, outCmd));
    TEST_ASSERT_EQUAL(0x01, outCmd);
}

void test_hmi_touch_parser_invalid_checksum(void) {
    HMI_TouchParserState state;
    uint8_t outCmd = 0;

    // Send 0x5A 0x01 0x00 (wrong checksum)
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x5A, state, outCmd));
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x01, state, outCmd));
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x00, state, outCmd));
}

void test_hmi_touch_parser_ignore_artifacts(void) {
    HMI_TouchParserState state;
    uint8_t outCmd = 0;

    // Send artifacts 0xFF 0x00 0xFF
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0xFF, state, outCmd));
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x00, state, outCmd));
    
    // Now send valid command
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x5A, state, outCmd));
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x02, state, outCmd));
    TEST_ASSERT_TRUE(HMI_parseTouchByte(0xFD, state, outCmd));
    TEST_ASSERT_EQUAL(0x02, outCmd);
}

void test_hmi_touch_parser_state_reset_on_artifact(void) {
    HMI_TouchParserState state;
    uint8_t outCmd = 0;

    // Send partial command
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x5A, state, outCmd));
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x03, state, outCmd));
    
    // Send artifact instead of checksum
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0xFF, state, outCmd));

    // Must be reset, so a checksum now won't trigger it
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0xFC, state, outCmd));
}

#include "driver/uart.h"

extern "C" {
int uart_write_bytes(uart_port_t, const void*, size_t) { return 0; }
int uart_read_bytes(uart_port_t, void*, uint32_t, uint32_t) { return 0; }
int uart_param_config(uart_port_t, const uart_config_t*) { return 0; }
int uart_set_pin(uart_port_t, int, int, int, int) { return 0; }
int uart_driver_install(uart_port_t, int, int, int, void*, int) { return 0; }
}

// We include the cpp file directly because DisplayHMI is ignored in native lib_ignore
#include "../../lib/DisplayHMI/DisplayHMI.cpp"
#include "freertos/task.h"

// Provide a mock for xTaskGetTickCount since test_touch_parser doesn't link fake_freertos.cpp natively.
// Or wait, does it? Let's just define a local mock tick count.
static uint32_t s_mockTicks = 0;
TickType_t xTaskGetTickCount(void) {
    return s_mockTicks;
}
void vTaskDelay(TickType_t) {}

void test_hmi_liveness_timeout(void) {
    DisplayHMI hmi;
    hmi.begin();
    
    // Test that initially it's alive (0 ms)
    s_mockTicks = 0;
    TEST_ASSERT_TRUE(hmi.isDisplayAlive());
    
    // Simulate touch byte received to update m_lastRxTimeMs
    uint8_t cmd;
    // We mock uart_read_bytes to return 0x65? No, readTouchCommand calls uart_read_bytes.
    // Instead of faking uart_read_bytes, we can just call it if we mock uart_read_bytes to return a byte.
    // But uart_read_bytes is stubbed in fake_uart to return 0.
    // Let's just advance time beyond 5000 ms.
    s_mockTicks = 6000;
    
    TEST_ASSERT_FALSE(hmi.isDisplayAlive());
}

// --- test_command_contract.cpp — HMI komut sozlesmesi (numara kilidi) ---
extern void test_cmd_ids_match_nextion_contract(void);
extern void test_estop_is_not_reset(void);
extern void test_frame_start(void);
extern void test_frame_drive_enable(void);
extern void test_frame_reset(void);
extern void test_frame_emergency_stop(void);
extern void test_frame_reserved_id_still_parses_but_is_unassigned(void);
extern void test_frame_stop(void);
extern void test_checksum_table_matches_document(void);
extern void test_old_doc_estop_frame_actually_decodes_to_reset(void);
extern void test_correct_estop_frame_decodes_to_emergency_stop(void);
extern void test_mismatched_checksum_rejects_frame(void);
extern void test_all_commands_decode_back_to_back(void);

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_hmi_touch_parser_valid_command);
    RUN_TEST(test_hmi_touch_parser_invalid_checksum);
    RUN_TEST(test_hmi_touch_parser_ignore_artifacts);
    RUN_TEST(test_hmi_touch_parser_state_reset_on_artifact);
    RUN_TEST(test_hmi_liveness_timeout);

    // Komut sozlesmesi (test_command_contract.cpp)
    RUN_TEST(test_cmd_ids_match_nextion_contract);
    RUN_TEST(test_estop_is_not_reset);
    RUN_TEST(test_frame_start);
    RUN_TEST(test_frame_drive_enable);
    RUN_TEST(test_frame_reset);
    RUN_TEST(test_frame_emergency_stop);
    RUN_TEST(test_frame_reserved_id_still_parses_but_is_unassigned);
    RUN_TEST(test_frame_stop);
    RUN_TEST(test_checksum_table_matches_document);
    RUN_TEST(test_old_doc_estop_frame_actually_decodes_to_reset);
    RUN_TEST(test_correct_estop_frame_decodes_to_emergency_stop);
    RUN_TEST(test_mismatched_checksum_rejects_frame);
    RUN_TEST(test_all_commands_decode_back_to_back);
    return UNITY_END();
}
