#pragma once

#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class CanManager {
   private:
    twai_general_config_t g_config;
    twai_timing_config_t t_config;
    twai_filter_config_t f_config;
    bool isInitialized;

   public:
    CanManager(gpio_num_t tx_pin, gpio_num_t rx_pin);
    bool begin();
    bool sendTorqueCommand(uint16_t torqueValue);
    bool readMessage(twai_message_t& message);
};
