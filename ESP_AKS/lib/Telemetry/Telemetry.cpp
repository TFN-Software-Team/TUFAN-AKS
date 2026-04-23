#include "Telemetry.h"
#include "SystemConfig.h"
#include "driver/uart.h"
#include "esp_log.h"

#include <cstdio>

static constexpr const char* TAG = "Telemetry";

Telemetry::Telemetry() : TEL_isInitialized(false) {}

bool Telemetry::begin() {
    TEL_isInitialized = true;
    return true;
}

void Telemetry::sendStatus(const TelemetryData& TEL_data) {
    if (!TEL_isInitialized)
        return;

    char TEL_payload[160];
    const int TEL_payloadLength = snprintf(
        TEL_payload, sizeof(TEL_payload),
        "RPM=%u,TQ=%d,ME=%u,BSOC=%u,BCUR=%d,BTMP=%d,BPV=%u,BCV=%u,BE=%u\r\n",
        TEL_data.TEL_motorRpm, TEL_data.TEL_motorTorqueFeedback,
        TEL_data.TEL_motorErrorFlags, TEL_data.TEL_bmsSoc,
        TEL_data.TEL_bmsCurrentDeciA, TEL_data.TEL_bmsTemperatureC,
        TEL_data.TEL_bmsPackVoltageDeciV,
        TEL_data.TEL_bmsAverageCellVoltageMv, TEL_data.TEL_bmsErrorFlags);

    if (TEL_payloadLength <= 0)
        return;

    const int TEL_txLength =
        (TEL_payloadLength < static_cast<int>(sizeof(TEL_payload)))
            ? TEL_payloadLength
            : static_cast<int>(sizeof(TEL_payload) - 1);

    const int TEL_written =
        uart_write_bytes(LORA_UART_NUM, TEL_payload, TEL_txLength);
    if (TEL_written < 0) {
        ESP_LOGE(TAG, "Telemetry TX failed");
    }
}
