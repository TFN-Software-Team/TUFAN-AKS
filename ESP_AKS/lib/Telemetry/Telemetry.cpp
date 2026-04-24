#include "Telemetry.h"
#include "SystemConfig.h"
#include "driver/uart.h"
#include "esp_log.h"

#include <cstdio>

static constexpr const char* TAG = "Telemetry";

Telemetry::Telemetry() : TEL_isInitialized(false), TEL_sequenceCounter(0) {}

bool Telemetry::begin() {
    TEL_isInitialized = true;
    TEL_sequenceCounter = 0;
    return true;
}

void Telemetry::sendStatus(const TelemetryData& TEL_data) {
    if (!TEL_isInitialized)
        return;

    char TEL_payload[160];
    const int TEL_payloadLength = snprintf(
        TEL_payload, sizeof(TEL_payload),
        "TEL,%d,%lu,%u,%d,%u,%u,%u,%u,%d,%d,%u,%u,%u,%u\r\n",
        LORA_PROTOCOL_VERSION,
        static_cast<unsigned long>(TEL_sequenceCounter),
        TEL_data.TEL_motorRpm, TEL_data.TEL_motorTorqueFeedback,
        TEL_data.TEL_motorErrorFlags,
        TEL_data.TEL_motorDataValid ? 1u : 0u,
        TEL_data.TEL_motorTimeoutActive ? 1u : 0u,
        TEL_data.TEL_bmsSoc,
        TEL_data.TEL_bmsCurrentDeciA, TEL_data.TEL_bmsTemperatureC,
        TEL_data.TEL_bmsPackVoltageDeciV,
        TEL_data.TEL_bmsAverageCellVoltageMv, TEL_data.TEL_bmsErrorFlags,
        TEL_data.TEL_bmsDataValid ? 1u : 0u);

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
        return;
    }

    TEL_sequenceCounter++;
}
