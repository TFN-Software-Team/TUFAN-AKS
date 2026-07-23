#include "unity.h"
#include "HMIMappings.h"
#include "SystemConfig.h"

static bool fake_relay_state[RELAY_TOTAL_CHANNELS] = {false};

static bool mock_read_relay(uint8_t channel) {
    if (channel < RELAY_TOTAL_CHANNELS) return fake_relay_state[channel];
    return false;
}

void test_hmi_are_all_contactors_closed_roles_1_idle(void) {
    for (int i = 0; i < RELAY_TOTAL_CHANNELS; i++) fake_relay_state[i] = false;
    
    // IDLE'da CONTACTOR_BANK_MASK icindeki tum kanallar kapali olmali (far, fan, flasor haric)
    for (int i = 0; i < RELAY_TOTAL_CHANNELS; i++) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << i)) {
            fake_relay_state[i] = true;
        }
    }
    
    TEST_ASSERT_TRUE(HMI_areAllContactorsClosed(VcuLogic::VcuState::IDLE, mock_read_relay));
    
    // S1 aciksa false (S1 bank maskesi icinde)
    fake_relay_state[RELAY_CH_S1_CHARGE] = false;
    TEST_ASSERT_FALSE(HMI_areAllContactorsClosed(VcuLogic::VcuState::IDLE, mock_read_relay));
}

void test_hmi_are_all_contactors_closed_roles_1_ready(void) {
    for (int i = 0; i < RELAY_TOTAL_CHANNELS; i++) fake_relay_state[i] = false;
    
    // READY'de DRIVE_BANK_MASK kullanilir. S1 bu maskenin DISINDADIR.
    for (int i = 0; i < RELAY_TOTAL_CHANNELS; i++) {
        if (RELAY_DRIVE_BANK_MASK & (1u << i)) {
            fake_relay_state[i] = true;
        }
    }
    
    fake_relay_state[RELAY_CH_S1_CHARGE] = false; // S1 acik olsa bile HMI Closed donmeli
    
    TEST_ASSERT_TRUE(HMI_areAllContactorsClosed(VcuLogic::VcuState::READY, mock_read_relay));
    
    // S2 acilirsa false donmeli
    fake_relay_state[RELAY_CH_S2_DRIVE] = false;
    TEST_ASSERT_FALSE(HMI_areAllContactorsClosed(VcuLogic::VcuState::READY, mock_read_relay));
}
