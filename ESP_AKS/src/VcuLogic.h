#pragma once
#include <cstdint>
#include "Telemetry.h"

namespace VcuLogic {

enum class VcuState : uint8_t {
    INIT = 0,
    IDLE = 1,
    READY = 2,  // Contactors closed, HV bus live
    DRIVE = 3,
    EMERGENCY_STOP = 4,
    FAULT = 5
};

enum class VcuEvent : uint8_t {
    NONE = 0,
    START_REQUEST = 1,
    DRIVE_ENABLE = 2,
    EMERGENCY_STOP = 3,
    FAULT_DETECTED = 4,
    RESET = 5
};

void init();
void run();
void postEvent(VcuEvent event);
VcuState getState();
void setTelemetryData(const TelemetryData& TEL_data);

}  // namespace VcuLogic
