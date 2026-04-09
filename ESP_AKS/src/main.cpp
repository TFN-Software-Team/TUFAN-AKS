#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "CanManager.h"
#include "SystemConfig.h"
#include "VcuLogic.h"

static constexpr const char* TAG = "APP_MAIN";

QueueHandle_t sensorDataQueue = nullptr;

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

void vTask_VCU_Logic(void* pvParameters) {
    while (true) {
        VcuLogic::run();
        vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz loop
    }
}

void vTask_HMI_Display(void* pvParameters) {
    while (true) {
        // TODO: Update display from sensorDataQueue and state information
        vTaskDelay(pdMS_TO_TICKS(100));  // 10Hz refresh
    }
}

extern "C" void app_main() {
    sensorDataQueue = xQueueCreate(10, sizeof(uint32_t));
    if (sensorDataQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create sensorDataQueue");
        return;
    }

    xTaskCreatePinnedToCore(vTask_CAN_Comm, "CAN_Task", 4096, nullptr, 5,
                            nullptr, 0);
    xTaskCreatePinnedToCore(vTask_HMI_Display, "HMI_Task", 4096, nullptr, 2,
                            nullptr, 0);
    xTaskCreatePinnedToCore(vTask_VCU_Logic, "VCU_Task", 4096, nullptr, 10,
                            nullptr, 1);
}
