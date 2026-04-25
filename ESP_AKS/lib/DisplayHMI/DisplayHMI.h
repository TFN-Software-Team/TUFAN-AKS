#pragma once
#include <cstddef>
#include <cstdint>

enum class HMI_VcuState : uint8_t {
    INIT = 0,
    IDLE = 1,
    READY = 2,
    DRIVE = 3,
    EMERGENCY_STOP = 4,
    FAULT = 5
};

struct HMI_DisplayData {
    uint16_t HMI_currentSpeed;
    uint8_t HMI_currentBattery;
    uint16_t HMI_motorRpm;
    int16_t HMI_motorTorqueFeedback;
    uint8_t HMI_motorErrorFlags;
    bool HMI_motorDataValid;
    bool HMI_motorTimeoutActive;
    int16_t HMI_bmsTemperatureC;
    uint16_t HMI_bmsPackVoltageDeciV;
    bool HMI_contactorClosed;
    HMI_VcuState HMI_vcuState;
};

class DisplayHMI {
   private:
    bool HMI_isInitialized;
    bool HMI_hasCachedScreen;
    HMI_DisplayData HMI_lastScreenData;

    void HMI_sendEndBytes();
    void HMI_drainRxBuffer();
    void HMI_sendNumericIfChanged(const char* HMI_component, int32_t HMI_value,
                                  int32_t HMI_lastValue, bool HMI_force);
    void HMI_sendTextIfChanged(const char* HMI_component,
                               const char* HMI_value,
                               const char* HMI_lastValue,
                               bool HMI_force);
    const char* HMI_getStateText(HMI_VcuState HMI_state) const;
    void HMI_formatErrorText(uint8_t HMI_errorFlags,
                             char* HMI_output,
                             size_t HMI_outputSize) const;
    const char* HMI_getValidityText(bool HMI_dataValid,
                                    bool HMI_timeoutActive) const;
    const char* HMI_getContactorText(bool HMI_contactorClosed) const;

   public:
    DisplayHMI();
    bool begin();
    void updateScreen(const HMI_DisplayData& HMI_data);
    bool readTouchCommand(uint8_t& HMI_command);
};
