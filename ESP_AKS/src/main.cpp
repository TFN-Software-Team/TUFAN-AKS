#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "CanManager.h"
#include "SystemConfig.h"
#include "VcuLogic.h"

static constexpr const char* TAG = "APP_MAIN";

QueueHandle_t sensorDataQueue = nullptr;

// CAN communication task — reads sensor data and sends torque commands
void vTask_CAN_Comm(void* pvParameters) {
    CanManager can(CAN_TX_PIN, CAN_RX_PIN);
    if (!can.begin()) {
        ESP_LOGE(TAG, "Failed to initialize CAN bus");
        vTaskDelete(nullptr);
        return;
    }

    uint16_t torqueValue = 0;
    while (true) {
        // TODO: Read motor controller status and publish sensor data to queue
        if (sensorDataQueue != nullptr) {
            xQueueSend(sensorDataQueue, &torqueValue, pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz loop
    }
}

// VCU logic task — runs the main state machine
void vTask_VCU_Logic(void* pvParameters) {
    while (true) {
        VcuLogic::run();
        vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz loop
    }
}

// HMI task — updates display with current state and sensor data
void vTask_HMI_Display(void* pvParameters) {
    while (true) {
        // TODO: Update display from sensorDataQueue and state information
        vTaskDelay(pdMS_TO_TICKS(100));  // 10Hz refresh
    }
}

// LoRa UART task — reads UKS commands and posts events to VcuLogic
void vTask_LoRa_UKS(void* pvParameters) {
    // UART init for E32
    uart_config_t uart_cfg = {
        .baud_rate = LORA_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(LORA_UART_NUM, &uart_cfg);
    uart_set_pin(LORA_UART_NUM, LORA_TX_PIN, LORA_RX_PIN, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
    uart_driver_install(LORA_UART_NUM, 256, 0, 0, nullptr, 0);

    uint8_t buf[4];
    while (true) {
        int len = uart_read_bytes(LORA_UART_NUM, buf, sizeof(buf),
                                  pdMS_TO_TICKS(100));
        if (len > 0) {
            switch (buf[0]) {
                case UKS_CMD_EMERGENCY_STOP:
                    VcuLogic::postEvent(VcuLogic::VcuEvent::EMERGENCY_STOP);
                    break;
                case UKS_CMD_START:
                    VcuLogic::postEvent(VcuLogic::VcuEvent::START_REQUEST);
                    break;
                case UKS_CMD_STOP:
                    VcuLogic::postEvent(VcuLogic::VcuEvent::RESET);
                    break;
                default:
                    break;
            }
        }
    }
}

// Main application entry point
extern "C" void app_main() {
    sensorDataQueue = xQueueCreate(10, sizeof(uint32_t));
    if (sensorDataQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create sensorDataQueue");
        return;
    }

    xTaskCreatePinnedToCore(vTask_CAN_Comm, "CAN_Task", 4096, nullptr, 5,
                            nullptr, 0);
    xTaskCreatePinnedToCore(vTask_HMI_Display, "HMI_Task", 2048, nullptr, 2,
                            nullptr, 0);
    xTaskCreatePinnedToCore(vTask_VCU_Logic, "VCU_Task", 2048, nullptr, 10,
                            nullptr, 1);
    xTaskCreatePinnedToCore(vTask_LoRa_UKS, "LoRa_Task", 2048, nullptr, 8,
                            nullptr, 0);
}
