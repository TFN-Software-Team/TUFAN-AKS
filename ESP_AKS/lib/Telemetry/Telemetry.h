#pragma once

#include <cstdint>

struct TelemetryData {
    uint16_t TEL_motorRpm;
    int16_t TEL_motorTorqueFeedback;
    uint8_t TEL_motorErrorFlags;
    bool TEL_motorDataValid;
    bool TEL_motorTimeoutActive;

    uint8_t TEL_bmsSoc;
    int16_t TEL_bmsCurrentDeciA;
    int16_t TEL_bmsTemperatureC;
    uint16_t TEL_bmsPackVoltageDeciV;
    uint16_t TEL_bmsAverageCellVoltageMv;
    uint8_t TEL_bmsErrorFlags;
    bool TEL_bmsDataValid;
};

class Telemetry {
   public:
    Telemetry();
    bool begin();
    void sendStatus(const TelemetryData& TEL_data);

   private:
    bool TEL_isInitialized;
};
