#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// Motor status received from motor driver via CAN
struct MotorStatus {
    uint16_t rpm;            // Motor RPM
    int16_t torqueFeedback;  // Actual torque feedback
    uint8_t errorFlags;      // Motor driver error flags
    bool isValid;            // True if data received recently
};

class CanManager {
   public:
    CanManager(gpio_num_t tx_pin, gpio_num_t rx_pin);

    bool begin();

    // Send torque command to motor driver (CAN ID: 0x100)
    bool sendTorqueCommand(uint16_t torqueValue);

    // Dispatch one received message — call this in the CAN task loop
    void processRxMessages();

    // Thread-safe read of latest motor status
    MotorStatus getMotorStatus() const;

   private:
    void handleMotorStatus(const twai_message_t& msg);

    twai_general_config_t g_config;
    twai_timing_config_t t_config;
    twai_filter_config_t f_config;
    bool isInitialized = false;

    MotorStatus s_motorStatus = {};
    mutable SemaphoreHandle_t s_mutex = nullptr;
};