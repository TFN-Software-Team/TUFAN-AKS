#include "CanManager.h"
#include "../../src/VcuLogic.h"
#include "SystemConfig.h"
#include "esp_err.h"
#include "esp_log.h"

static constexpr const char* TAG = "CanManager";

CanManager::CanManager(gpio_num_t tx_pin, gpio_num_t rx_pin)
    : g_config(TWAI_GENERAL_CONFIG_DEFAULT(tx_pin, rx_pin, TWAI_MODE_NORMAL)),
      t_config(TWAI_TIMING_CONFIG_500KBITS()),
      f_config(TWAI_FILTER_CONFIG_ACCEPT_ALL()) {}

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
        return false;
    }

    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_start failed: %s", esp_err_to_name(err));
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

            case CAN_ID_BMS_STATUS:
                // BMS is out of scope — log and ignore
                ESP_LOGD(TAG, "BMS message received (ignored)");
                break;

            default:
                ESP_LOGD(TAG, "Unknown CAN ID: 0x%03lX", msg.identifier);
                break;
        }
    }
}

MotorStatus CanManager::getMotorStatus() const {
    MotorStatus copy;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    copy = s_motorStatus;
    xSemaphoreGive(s_mutex);
    return copy;
}

void CanManager::handleMotorStatus(const twai_message_t& msg) {
    if (msg.data_length_code < 4) {
        ESP_LOGW(TAG, "Motor status DLC too short: %d", msg.data_length_code);
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    s_motorStatus.rpm = (msg.data[0] << 8) | msg.data[1];
    s_motorStatus.torqueFeedback = (msg.data[2] << 8) | msg.data[3];
    s_motorStatus.errorFlags = (msg.data_length_code >= 5) ? msg.data[4] : 0;
    s_motorStatus.isValid = true;

    xSemaphoreGive(s_mutex);

    // If motor reports an error, notify VCU
    if (s_motorStatus.errorFlags != 0) {
        ESP_LOGW(TAG, "Motor error flags: 0x%02X", s_motorStatus.errorFlags);
        VcuLogic::postEvent(VcuLogic::VcuEvent::FAULT_DETECTED);
    }

    ESP_LOGD(TAG, "Motor: RPM=%d, Torque=%d", s_motorStatus.rpm,
             s_motorStatus.torqueFeedback);
}