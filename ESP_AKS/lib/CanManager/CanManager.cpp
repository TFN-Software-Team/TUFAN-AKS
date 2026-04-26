#include "CanManager.h"
#include "SystemConfig.h"
#include "esp_err.h"
#include "esp_log.h"

static constexpr const char* TAG = "CanManager";

CanManager::CanManager(gpio_num_t tx_pin, gpio_num_t rx_pin)
    : g_config(TWAI_GENERAL_CONFIG_DEFAULT(tx_pin, rx_pin, TWAI_MODE_NORMAL)),
      t_config(TWAI_TIMING_CONFIG_500KBITS()),
      f_config(TWAI_FILTER_CONFIG_ACCEPT_ALL()) {}

void CanManager::setEventCallback(CAN_EventCallback CAN_callback,
                                  void* CAN_context) {
    CAN_eventCallback = CAN_callback;
    CAN_eventContext = CAN_context;
}

bool CanManager::begin() {
    if (isInitialized)
        return true;

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(s_mutex);
        s_mutex = nullptr;
        return false;
    }

    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_start failed: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        vSemaphoreDelete(s_mutex);
        s_mutex = nullptr;
        return false;
    }

    isInitialized = true;
    ESP_LOGI(TAG, "CAN initialized at 500kbps");
    return true;
}

bool CanManager::sendTorqueCommand(uint16_t torqueValue) {
    if (!isInitialized)
        return false;

    twai_message_t msg = {};
    msg.identifier = CAN_ID_TORQUE_CMD;
    msg.data_length_code = 2;
    msg.data[0] = static_cast<uint8_t>(torqueValue >> 8);
    msg.data[1] = static_cast<uint8_t>(torqueValue & 0xFF);
    msg.flags = TWAI_MSG_FLAG_NONE;

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(10));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Torque TX failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void CanManager::processRxMessages() {
    if (!isInitialized)
        return;

    twai_message_t msg;
    // Process up to 5 messages per call to avoid blocking the task
    for (int i = 0; i < 5; i++) {
        esp_err_t err = twai_receive(&msg, 0);  // non-blocking
        if (err == ESP_ERR_TIMEOUT)
            break;  // no more messages
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "RX error: %s", esp_err_to_name(err));
            break;
        }

        switch (msg.identifier) {
            case CAN_ID_MOTOR_STATUS:
                handleMotorStatus(msg);
                break;

            case CAN_ID_BMS_CONFIG:
                handleBmsConfig(msg);
                break;

            case CAN_ID_BMS_LIVE:
                handleBmsLive(msg);
                break;

            case CAN_ID_BMS_STATUS:
                ESP_LOGD(TAG, "Legacy BMS frame received: 0x%03lX",
                         msg.identifier);
                break;

            default:
                ESP_LOGD(TAG, "Unknown CAN ID: 0x%03lX", msg.identifier);
                break;
        }
    }

    updateMotorStatusValidity();
}

MotorStatus CanManager::getMotorStatus() const {
    if (s_mutex == nullptr)
        return s_motorStatus;

    MotorStatus CAN_statusCopy = {};
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    CAN_statusCopy = s_motorStatus;
    xSemaphoreGive(s_mutex);
    return CAN_statusCopy;
}

TelemetryData CanManager::getTelemetryData() const {
    if (s_mutex == nullptr)
        return s_telemetryData;

    TelemetryData CAN_telemetryCopy = {};
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    CAN_telemetryCopy = s_telemetryData;
    xSemaphoreGive(s_mutex);
    return CAN_telemetryCopy;
}

void CanManager::handleMotorStatus(const twai_message_t& msg) {
    MotorStatus parsed{};
    if (!CanParse::parseMotorStatus(msg, parsed)) {
        ESP_LOGW(TAG, "Motor status DLC too short: %d", msg.data_length_code);
        return;
    }

    if (s_mutex == nullptr) {
        ESP_LOGW(TAG, "Motor status received before mutex initialization");
        return;
    }

    uint8_t CAN_previousMotorErrorFlags = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    CAN_previousMotorErrorFlags = s_motorStatus.errorFlags;
    s_motorStatus = parsed;
    CAN_lastMotorStatusTick = xTaskGetTickCount();
    CAN_hasSeenMotorStatus = true;
    CAN_motorTimeoutLogged = false;

    s_telemetryData.TEL_motorRpm = s_motorStatus.rpm;
    s_telemetryData.TEL_motorTorqueFeedback = s_motorStatus.torqueFeedback;
    s_telemetryData.TEL_motorErrorFlags = s_motorStatus.errorFlags;
    s_telemetryData.TEL_motorDataValid = s_motorStatus.isValid;
    s_telemetryData.TEL_motorTimeoutActive = false;

    xSemaphoreGive(s_mutex);

    notifyFaultIfNeeded(CAN_previousMotorErrorFlags, s_motorStatus.errorFlags,
                        "Motor");

    ESP_LOGD(TAG, "Motor: RPM=%d, Torque=%d", s_motorStatus.rpm,
             s_motorStatus.torqueFeedback);
}

void CanManager::handleBmsConfig(const twai_message_t& msg) {
    if (s_mutex == nullptr) {
        ESP_LOGW(TAG, "BMS config received before mutex initialization");
        return;
    }

    TelemetryData parsed{};
    if (!CanParse::parseBmsConfig(msg, parsed)) {
        ESP_LOGW(TAG, "BMS config DLC too short: %d", msg.data_length_code);
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    s_telemetryData.TEL_bmsPackVoltageDeciV = parsed.TEL_bmsPackVoltageDeciV;
    s_telemetryData.TEL_bmsAverageCellVoltageMv =
        parsed.TEL_bmsAverageCellVoltageMv;
    s_telemetryData.TEL_bmsDataValid = true;

    xSemaphoreGive(s_mutex);

    ESP_LOGD(TAG, "BMS config: pack=%u deciV, cell=%u mV",
             parsed.TEL_bmsPackVoltageDeciV,
             parsed.TEL_bmsAverageCellVoltageMv);
}

void CanManager::handleBmsLive(const twai_message_t& msg) {
    if (s_mutex == nullptr) {
        ESP_LOGW(TAG, "BMS live received before mutex initialization");
        return;
    }

    TelemetryData parsed{};
    if (!CanParse::parseBmsLive(msg, parsed)) {
        ESP_LOGW(TAG, "BMS live DLC too short: %d", msg.data_length_code);
        return;
    }

    uint8_t CAN_previousBmsErrorFlags = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    CAN_previousBmsErrorFlags = s_telemetryData.TEL_bmsErrorFlags;

    s_telemetryData.TEL_bmsErrorFlags = parsed.TEL_bmsErrorFlags;
    s_telemetryData.TEL_bmsCurrentDeciA = parsed.TEL_bmsCurrentDeciA;
    s_telemetryData.TEL_bmsTemperatureC = parsed.TEL_bmsTemperatureC;
    s_telemetryData.TEL_bmsSoc = parsed.TEL_bmsSoc;
    s_telemetryData.TEL_bmsDataValid = true;

    xSemaphoreGive(s_mutex);

    notifyFaultIfNeeded(CAN_previousBmsErrorFlags, parsed.TEL_bmsErrorFlags,
                        "BMS");

    ESP_LOGD(TAG, "BMS live: current=%d deciA, temp=%d C, soc=%u%%",
             parsed.TEL_bmsCurrentDeciA, parsed.TEL_bmsTemperatureC,
             parsed.TEL_bmsSoc);
}

void CanManager::updateMotorStatusValidity() {
    if (s_mutex == nullptr)
        return;

    const TickType_t CAN_nowTick = xTaskGetTickCount();
    bool CAN_shouldLogTimeout = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (CanParse::isMotorStatusTimedOut(
            CAN_hasSeenMotorStatus, s_motorStatus.isValid, CAN_nowTick,
            CAN_lastMotorStatusTick,
            pdMS_TO_TICKS(CAN_MOTOR_STATUS_TIMEOUT_MS))) {
        s_motorStatus.isValid = false;
        s_telemetryData.TEL_motorDataValid = false;
        s_telemetryData.TEL_motorTimeoutActive = true;
        CAN_shouldLogTimeout = !CAN_motorTimeoutLogged;
        CAN_motorTimeoutLogged = true;
    }

    xSemaphoreGive(s_mutex);

    if (CAN_shouldLogTimeout) {
        ESP_LOGW(TAG, "Motor status timeout after %d ms",
                 CAN_MOTOR_STATUS_TIMEOUT_MS);
    }
}

void CanManager::notifyFaultIfNeeded(uint8_t CAN_previousFlags,
                                     uint8_t CAN_currentFlags,
                                     const char* CAN_faultSource) {
    if (CAN_currentFlags == 0 || CAN_currentFlags == CAN_previousFlags)
        return;

    ESP_LOGW(TAG, "%s error flags: 0x%02X", CAN_faultSource, CAN_currentFlags);
    if (CAN_eventCallback != nullptr) {
        CAN_eventCallback(CAN_Event::FAULT_DETECTED, CAN_eventContext);
    }
}
