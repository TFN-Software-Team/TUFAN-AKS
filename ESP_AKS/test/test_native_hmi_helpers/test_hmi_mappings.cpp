#include "unity.h"
#include "HMIMappings.h"
#include "SystemConfig.h"

static bool fake_relay_state[RELAY_TOTAL_CHANNELS] = {false};

static bool mock_read_relay(uint8_t channel) {
    if (channel < RELAY_TOTAL_CHANNELS) return fake_relay_state[channel];
    return false;
}

void test_hmi_are_all_contactors_closed_roles_0_idle(void) {
    for (int i = 0; i < RELAY_TOTAL_CHANNELS; i++) fake_relay_state[i] = true;
    fake_relay_state[0] = false;
    
    TEST_ASSERT_FALSE(HMI_areAllContactorsClosed(VcuLogic::VcuState::IDLE, mock_read_relay));
    
    fake_relay_state[0] = true;
    TEST_ASSERT_TRUE(HMI_areAllContactorsClosed(VcuLogic::VcuState::IDLE, mock_read_relay));
}

void test_hmi_are_all_contactors_closed_roles_0_ready(void) {
    for (int i = 0; i < RELAY_TOTAL_CHANNELS; i++) fake_relay_state[i] = true;
    fake_relay_state[0] = false;
    
    // RELAY_ROLES_ASSIGNED=0'da RELAY_DRIVE_BANK_MASK tanimli degil, hepsine bakar.
    TEST_ASSERT_FALSE(HMI_areAllContactorsClosed(VcuLogic::VcuState::READY, mock_read_relay));
    
    fake_relay_state[0] = true;
    TEST_ASSERT_TRUE(HMI_areAllContactorsClosed(VcuLogic::VcuState::READY, mock_read_relay));
}
