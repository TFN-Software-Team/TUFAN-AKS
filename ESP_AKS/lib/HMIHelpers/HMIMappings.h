#pragma once

#include "HMIHelpers.h"
#include "VcuLogic.h"
#include <cstddef>
#include <cstdint>

// VcuState -> HMI_VcuState mapping
HMI_VcuState HMI_mapVcuState(VcuLogic::VcuState HMI_state);

// Kontaktörlerin kapalı olup olmadığını VCU durumuna göre kontrol eder.
// currentState == READY veya DRIVE ise (ve RELAY_ROLES_ASSIGNED=1 ise) RELAY_DRIVE_BANK_MASK kullanılır.
// Diğer durumlarda RELAY_CONTACTOR_BANK_MASK kullanılır.
// readRelay callback'i ilgili kanalın fiziksel/mantıksal durumunu döndürmelidir.
bool HMI_areAllContactorsClosed(VcuLogic::VcuState currentState, bool (*readRelay)(uint8_t channel));

// 24-hücre BMS Nextion komutlarını HMI UART'ına yazan emit callback'i.
void BMS_emitNextionCommand(const char* BMS_cmd, size_t BMS_len, void* BMS_ctx);
