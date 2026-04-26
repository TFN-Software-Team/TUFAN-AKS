#pragma once
//
// Saf HMI helper'ları + Nextion UART komut yardımcıları.
// 4 saf metin dönüşümü inline; donanım bağımlılıkları yoktur.
// 3 UART helper'ı (sendEndBytes, sendNumericIfChanged, sendTextIfChanged)
// uart_write_bytes çağırdığı için CPP tarafında tanımlıdır.
// DisplayHMI sınıfı bu fonksiyonları çağırır; native testler aynı
// fonksiyonları doğrudan çağırarak metin formatlama ve cache mantığını
// izole eder.
//
#include <cstddef>
#include <cstdint>
#include <cstdio>

enum class HMI_VcuState : uint8_t {
    INIT = 0,
    IDLE = 1,
    READY = 2,
    DRIVE = 3,
    EMERGENCY_STOP = 4,
    FAULT = 5
};

inline const char* HMI_getStateText(HMI_VcuState HMI_state) {
    switch (HMI_state) {
        case HMI_VcuState::INIT:           return "INIT";
        case HMI_VcuState::IDLE:           return "IDLE";
        case HMI_VcuState::READY:          return "READY";
        case HMI_VcuState::DRIVE:          return "DRIVE";
        case HMI_VcuState::EMERGENCY_STOP: return "ESTOP";
        case HMI_VcuState::FAULT:          return "FAULT";
        default:                           return "UNK";
    }
}

inline void HMI_formatErrorText(uint8_t HMI_errorFlags, char* HMI_output,
                                size_t HMI_outputSize) {
    if (HMI_outputSize == 0)
        return;
    snprintf(HMI_output, HMI_outputSize, "0x%02X", HMI_errorFlags);
}

inline const char* HMI_getValidityText(bool HMI_dataValid,
                                       bool HMI_timeoutActive) {
    if (HMI_timeoutActive)
        return "TIMEOUT";
    return HMI_dataValid ? "VALID" : "INVALID";
}

inline const char* HMI_getContactorText(bool HMI_contactorClosed) {
    return HMI_contactorClosed ? "CLOSED" : "OPEN";
}

// Nextion UART tarafı — implementasyonu HMIHelpers.cpp içindedir.
void HMI_sendEndBytes(void);

// Yalnızca yeni değer önceki cache'ten farklıysa (veya force=true) UART'a
// "{component}.val={value}\xFF\xFF\xFF" yazar.
void HMI_sendNumericIfChanged(const char* HMI_component, int32_t HMI_value,
                              int32_t HMI_lastValue, bool HMI_force);

// Aynı şekilde text varyantı: "{component}.txt=\"{value}\"\xFF\xFF\xFF".
void HMI_sendTextIfChanged(const char* HMI_component, const char* HMI_value,
                           const char* HMI_lastValue, bool HMI_force);
