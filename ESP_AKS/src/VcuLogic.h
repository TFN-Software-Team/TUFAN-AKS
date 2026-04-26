#pragma once
#include <cstdint>
#include "SystemConfig.h"
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

// ---------------------------------------------------------------------------
// Pure safety predicates
// ---------------------------------------------------------------------------
// Donanım veya global state'ten bağımsız; doğrudan argümana bakar. Inline
// tutulduğu için saf mantık testlerinde VcuLogic.cpp linklenmeden
// çağrılabilir.
inline bool isCurrentCritical(int16_t TEL_bmsCurrentDeciA) {
    return TEL_bmsCurrentDeciA >= BMS_CRITICAL_MAX_CHARGE_CURRENT_DECI_A ||
           TEL_bmsCurrentDeciA <= -BMS_CRITICAL_MAX_DISCHARGE_CURRENT_DECI_A;
}

inline bool isCurrentWarning(int16_t TEL_bmsCurrentDeciA) {
    return TEL_bmsCurrentDeciA >= BMS_WARN_MAX_CHARGE_CURRENT_DECI_A ||
           TEL_bmsCurrentDeciA <= -BMS_WARN_MAX_DISCHARGE_CURRENT_DECI_A;
}

inline bool hasWarningCondition(const TelemetryData& VCU_data) {
    if (!VCU_data.TEL_bmsDataValid)
        return false;

    return VCU_data.TEL_bmsTemperatureC >= BMS_WARN_MAX_TEMP_C ||
           VCU_data.TEL_bmsPackVoltageDeciV <=
               BMS_WARN_MIN_PACK_VOLTAGE_DECI_V ||
           VCU_data.TEL_bmsPackVoltageDeciV >=
               BMS_WARN_MAX_PACK_VOLTAGE_DECI_V ||
           isCurrentWarning(VCU_data.TEL_bmsCurrentDeciA);
}

inline bool hasCriticalCondition(const TelemetryData& VCU_data,
                                 VcuState currentState) {
    if (VCU_data.TEL_motorErrorFlags != 0 || VCU_data.TEL_bmsErrorFlags != 0)
        return true;

    if (VCU_data.TEL_motorTimeoutActive && currentState != VcuState::IDLE)
        return true;

    if (!VCU_data.TEL_bmsDataValid)
        return false;

    return VCU_data.TEL_bmsTemperatureC >= BMS_CRITICAL_MAX_TEMP_C ||
           VCU_data.TEL_bmsPackVoltageDeciV <=
               BMS_CRITICAL_MIN_PACK_VOLTAGE_DECI_V ||
           VCU_data.TEL_bmsPackVoltageDeciV >=
               BMS_CRITICAL_MAX_PACK_VOLTAGE_DECI_V ||
           isCurrentCritical(VCU_data.TEL_bmsCurrentDeciA);
}

inline bool isResetInterlockSatisfied(const TelemetryData& VCU_data,
                                      VcuState currentState) {
    if (VCU_data.TEL_motorErrorFlags != 0 || VCU_data.TEL_bmsErrorFlags != 0)
        return false;

    if (hasCriticalCondition(VCU_data, currentState))
        return false;

    return true;
}

// ---------------------------------------------------------------------------
// Stateful API
// ---------------------------------------------------------------------------
void init();
void run();
void postEvent(VcuEvent event);
VcuState getState();
void setTelemetryData(const TelemetryData& TEL_data);

#ifdef VCU_LOGIC_TESTABLE
// Yalnız native test build'inde aktif. Tüm modül-içi state'i (durum, timer,
// son telemetri, queue) sıfırlar; setUp() içinde çağrılarak testler arası
// izolasyon sağlanır. Üretim build'inde tanımlı değildir.
void resetForTest();
#endif

}  // namespace VcuLogic
