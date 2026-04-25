#include "DisplayHMI.h"
#include "SystemConfig.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>

static constexpr const char *TAG = "DisplayHMI";

DisplayHMI::DisplayHMI()
    : HMI_isInitialized(false),
      HMI_hasCachedScreen(false),
      HMI_lastScreenData({}) {}

bool DisplayHMI::begin() {
    if (HMI_isInitialized) return true;

    uart_config_t HMI_uartConfig = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT
    };

    if (uart_param_config(HMI_UART_NUM, &HMI_uartConfig) != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed");
        return false;
    }

    if (uart_set_pin(HMI_UART_NUM, HMI_TX_PIN, HMI_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed (TX=%d, RX=%d)", HMI_TX_PIN, HMI_RX_PIN);
        return false;
    }

    if (uart_driver_install(HMI_UART_NUM, 256, 256, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed");
        return false;
    }

    HMI_isInitialized = true;

    // Nextion açılış mesajlarını temizle
    HMI_drainRxBuffer();

    // Nextion acknowledge yanıtlarını kapat (bkcmd=0)
    // Aksi halde her komut sonrası gelen 0x01/0x02/0x03 yanıtları
    // readTouchCommand tarafından sahte komut olarak yorumlanır
    const char *HMI_bkcmd = "bkcmd=0";
    uart_write_bytes(HMI_UART_NUM, HMI_bkcmd, 7);
    HMI_sendEndBytes();
    vTaskDelay(pdMS_TO_TICKS(50));  // Nextion'ın işlemesi için bekle
    HMI_drainRxBuffer();            // bkcmd komutunun kendi acknowledge'ını temizle

    ESP_LOGI(TAG, "Initialized on UART%d (TX=IO%d, RX=IO%d)", HMI_UART_NUM, HMI_TX_PIN, HMI_RX_PIN);
    return true;
}

void DisplayHMI::HMI_sendEndBytes() {
    const uint8_t HMI_endBytes[3] = {0xFF, 0xFF, 0xFF};
    uart_write_bytes(HMI_UART_NUM, (const char*)HMI_endBytes, 3);
}

void DisplayHMI::HMI_drainRxBuffer() {
    uint8_t HMI_drainBuf[32];
    while (uart_read_bytes(HMI_UART_NUM, HMI_drainBuf, sizeof(HMI_drainBuf), 0) > 0) {
        // Nextion acknowledge/error yanıtlarını temizle
    }
}

void DisplayHMI::HMI_sendNumericIfChanged(const char* HMI_component,
                                          int32_t HMI_value,
                                          int32_t HMI_lastValue,
                                          bool HMI_force) {
    if (!HMI_force && HMI_value == HMI_lastValue)
        return;

    char HMI_command[48];
    const int HMI_commandLen = snprintf(HMI_command, sizeof(HMI_command),
                                        "%s.val=%ld", HMI_component,
                                        static_cast<long>(HMI_value));
    if (HMI_commandLen <= 0)
        return;

    uart_write_bytes(HMI_UART_NUM, HMI_command, HMI_commandLen);
    HMI_sendEndBytes();
}

void DisplayHMI::HMI_sendTextIfChanged(const char* HMI_component,
                                       const char* HMI_value,
                                       const char* HMI_lastValue,
                                       bool HMI_force) {
    if (!HMI_force && std::strcmp(HMI_value, HMI_lastValue) == 0)
        return;

    char HMI_command[64];
    const int HMI_commandLen = snprintf(HMI_command, sizeof(HMI_command),
                                        "%s.txt=\"%s\"", HMI_component,
                                        HMI_value);
    if (HMI_commandLen <= 0)
        return;

    uart_write_bytes(HMI_UART_NUM, HMI_command, HMI_commandLen);
    HMI_sendEndBytes();
}

const char* DisplayHMI::HMI_getStateText(HMI_VcuState HMI_state) const {
    switch (HMI_state) {
        case HMI_VcuState::INIT:
            return "INIT";
        case HMI_VcuState::IDLE:
            return "IDLE";
        case HMI_VcuState::READY:
            return "READY";
        case HMI_VcuState::DRIVE:
            return "DRIVE";
        case HMI_VcuState::EMERGENCY_STOP:
            return "ESTOP";
        case HMI_VcuState::FAULT:
            return "FAULT";
        default:
            return "UNK";
    }
}

void DisplayHMI::HMI_formatErrorText(uint8_t HMI_errorFlags,
                                     char* HMI_output,
                                     size_t HMI_outputSize) const {
    if (HMI_outputSize == 0)
        return;
    snprintf(HMI_output, HMI_outputSize, "0x%02X", HMI_errorFlags);
}

const char* DisplayHMI::HMI_getValidityText(bool HMI_dataValid,
                                            bool HMI_timeoutActive) const {
    if (HMI_timeoutActive)
        return "TIMEOUT";
    return HMI_dataValid ? "VALID" : "INVALID";
}

const char* DisplayHMI::HMI_getContactorText(bool HMI_contactorClosed) const {
    return HMI_contactorClosed ? "CLOSED" : "OPEN";
}

void DisplayHMI::updateScreen(const HMI_DisplayData& HMI_data) {
    if (!HMI_isInitialized) return;

    const bool HMI_forceRefresh = !HMI_hasCachedScreen;
    char HMI_currentErrorText[16];
    char HMI_lastErrorText[16];

    HMI_formatErrorText(HMI_data.HMI_motorErrorFlags, HMI_currentErrorText,
                        sizeof(HMI_currentErrorText));
    HMI_formatErrorText(HMI_lastScreenData.HMI_motorErrorFlags,
                        HMI_lastErrorText, sizeof(HMI_lastErrorText));

    HMI_sendNumericIfChanged("speed", HMI_data.HMI_currentSpeed,
                             HMI_lastScreenData.HMI_currentSpeed,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("bat", HMI_data.HMI_currentBattery,
                             HMI_lastScreenData.HMI_currentBattery,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("rpm", HMI_data.HMI_motorRpm,
                             HMI_lastScreenData.HMI_motorRpm,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("torque", HMI_data.HMI_motorTorqueFeedback,
                             HMI_lastScreenData.HMI_motorTorqueFeedback,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("temp", HMI_data.HMI_bmsTemperatureC,
                             HMI_lastScreenData.HMI_bmsTemperatureC,
                             HMI_forceRefresh);
    HMI_sendNumericIfChanged("packv", HMI_data.HMI_bmsPackVoltageDeciV,
                             HMI_lastScreenData.HMI_bmsPackVoltageDeciV,
                             HMI_forceRefresh);

    HMI_sendTextIfChanged("state", HMI_getStateText(HMI_data.HMI_vcuState),
                          HMI_getStateText(HMI_lastScreenData.HMI_vcuState),
                          HMI_forceRefresh);
    HMI_sendTextIfChanged("motorErr", HMI_currentErrorText, HMI_lastErrorText,
                          HMI_forceRefresh);
    HMI_sendTextIfChanged("valid",
                          HMI_getValidityText(HMI_data.HMI_motorDataValid,
                                              HMI_data.HMI_motorTimeoutActive),
                          HMI_getValidityText(
                              HMI_lastScreenData.HMI_motorDataValid,
                              HMI_lastScreenData.HMI_motorTimeoutActive),
                          HMI_forceRefresh);
    HMI_sendTextIfChanged("contactor",
                          HMI_getContactorText(HMI_data.HMI_contactorClosed),
                          HMI_getContactorText(
                              HMI_lastScreenData.HMI_contactorClosed),
                          HMI_forceRefresh);

    HMI_lastScreenData = HMI_data;
    HMI_hasCachedScreen = true;
}

bool DisplayHMI::readTouchCommand(uint8_t& HMI_command) {
    if (!HMI_isInitialized) return false;

    uint8_t HMI_rxBuf[1];
    int HMI_rxBytes = uart_read_bytes(HMI_UART_NUM, HMI_rxBuf, 1, pdMS_TO_TICKS(10));

    if (HMI_rxBytes <= 0) return false;

    // Gürültü byte'larını filtrele:
    // 0xFF = Nextion end-byte (buffer'da kalan artık)
    // 0x00 = Nextion Invalid Instruction yanıtı
    // bkcmd=0 sayesinde 0x01-0x05 ack yanıtları gelmez,
    // dolayısıyla bu değerler güvenle komut olarak yorumlanabilir
    if (HMI_rxBuf[0] == 0xFF || HMI_rxBuf[0] == 0x00) return false;

    HMI_command = HMI_rxBuf[0];
    return true;
}
