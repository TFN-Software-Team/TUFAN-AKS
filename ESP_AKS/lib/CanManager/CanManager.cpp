#include "CanManager.h"
#include "esp_err.h"
#include "esp_log.h"

static constexpr const char* TAG = "CanManager";

CanManager::CanManager(gpio_num_t tx_pin, gpio_num_t rx_pin)
    : g_config(TWAI_GENERAL_CONFIG_DEFAULT(tx_pin, rx_pin, TWAI_MODE_NORMAL)),
      t_config(TWAI_TIMING_CONFIG_500KBITS()),
      f_config(TWAI_FILTER_CONFIG_ACCEPT_ALL()),
      isInitialized(false) {}

bool CanManager::begin() {
    if (isInitialized) {
        return true;
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
    ESP_LOGI(TAG, "CAN initialized");
    return true;
}

bool CanManager::sendTorqueCommand(uint16_t torqueValue) {
    if (!isInitialized) {
        return false;
    }

    twai_message_t message;
    message.identifier = 0x100;
    message.data_length_code = 2;
    message.data[0] = static_cast<uint8_t>(torqueValue >> 8);
    message.data[1] = static_cast<uint8_t>(torqueValue & 0xFF);
    message.flags = TWAI_MSG_FLAG_NONE;

    esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(100));
    return (err == ESP_OK);
}

bool CanManager::readMessage(twai_message_t& message) {
    if (!isInitialized) {
        return false;
    }

    esp_err_t err = twai_receive(&message, pdMS_TO_TICKS(100));
    return (err == ESP_OK);
}
