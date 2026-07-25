#include "HMIMappings.h"
#include "SystemConfig.h"
#include "driver/uart.h"

HMI_VcuState HMI_mapVcuState(VcuLogic::VcuState HMI_state) {
    switch (HMI_state) {
        case VcuLogic::VcuState::INIT:
            return HMI_VcuState::INIT;
        case VcuLogic::VcuState::IDLE:
            return HMI_VcuState::IDLE;
        case VcuLogic::VcuState::READY:
            return HMI_VcuState::READY;
        case VcuLogic::VcuState::DRIVE:
            return HMI_VcuState::DRIVE;
        case VcuLogic::VcuState::EMERGENCY_STOP:
            return HMI_VcuState::EMERGENCY_STOP;
        case VcuLogic::VcuState::FAULT:
            return HMI_VcuState::FAULT;
        default:
            return HMI_VcuState::FAULT;
    }
}

bool HMI_areAllContactorsClosed(VcuLogic::VcuState currentState, bool (*readRelay)(uint8_t channel)) {
    uint16_t mask = RELAY_CONTACTOR_BANK_MASK;
    if (currentState == VcuLogic::VcuState::READY || currentState == VcuLogic::VcuState::DRIVE) {
#if RELAY_ROLES_ASSIGNED
        mask = RELAY_DRIVE_BANK_MASK;
#else
        // RELAY_ROLES_ASSIGNED=0 iken RELAY_DRIVE_BANK_MASK tanimli degil, 
        // RELAY_CONTACTOR_BANK_MASK zaten 10 kanalin tamamidir.
        mask = RELAY_CONTACTOR_BANK_MASK;
#endif
    }

    for (uint8_t REL_channel = 0; REL_channel < RELAY_TOTAL_CHANNELS; ++REL_channel) {
        if (!(mask & (1u << REL_channel))) {
            continue;  // maske disi kanal (flasor, vb.) gosterge kapsamina girmez
        }
        if (!readRelay(REL_channel)) {
            return false;
        }
    }
    return true;
}

void BMS_emitNextionCommand(const char* BMS_cmd, size_t BMS_len, void* BMS_ctx) {
    (void)BMS_ctx;
    uart_write_bytes(HMI_UART_NUM, BMS_cmd, BMS_len);
    HMI_sendEndBytes();
}
