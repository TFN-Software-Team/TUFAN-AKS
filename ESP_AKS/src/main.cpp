#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "DisplayHMI.h"
#include "CanManager.h"
#include "RelayManager.h"
#include "SystemConfig.h"
#include "VcuLogic.h"

static constexpr const char *TAG = "APP_MAIN";

// Stack high-water-mark logging interval (ticks)
static constexpr uint32_t STACK_LOG_INTERVAL_MS = 30000;

QueueHandle_t sensorDataQueue = nullptr;

// ---------------------------------------------------------------------------
// Helper: log stack high-water-mark periodically
// ---------------------------------------------------------------------------
static void logStackUsage(const char *taskName, uint32_t &lastLogTick) {
  uint32_t now = xTaskGetTickCount();
  if ((now - lastLogTick) >= pdMS_TO_TICKS(STACK_LOG_INTERVAL_MS)) {
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGD(taskName, "Stack high water mark: %u words remaining", hwm);
    lastLogTick = now;
  }
}

// ---------------------------------------------------------------------------
// CAN communication task
// ---------------------------------------------------------------------------
void vTask_CAN_Comm(void *pvParameters) {
  esp_task_wdt_add(nullptr);

  CanManager can(CAN_TX_PIN, CAN_RX_PIN);

  if (!can.begin()) {
    ESP_LOGE(TAG, "Failed to initialize CAN bus");
    esp_task_wdt_delete(nullptr);
    vTaskDelete(nullptr);
    return;
  }

  uint16_t torqueCmd = 0;
  uint32_t lastStackLog = 0;

  while (true) {
    esp_task_wdt_reset();

    // 1. Read incoming messages and dispatch them
    can.processRxMessages();

    // 2. Send torque command if in DRIVE state
    if (VcuLogic::getState() == VcuLogic::VcuState::DRIVE) {
      // TODO: Get actual torque value from control logic
      can.sendTorqueCommand(torqueCmd);
    } else {
      // Not in drive — send zero torque for safety
      can.sendTorqueCommand(0);
    }

    // 3. Push motor status to HMI queue
    if (sensorDataQueue != nullptr) {
      MotorStatus status = can.getMotorStatus();
      xQueueOverwrite(sensorDataQueue, &status.rpm);
    }

    logStackUsage("CAN_Task", lastStackLog);
    vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz
  }
}

// ---------------------------------------------------------------------------
// VCU logic task — runs the main state machine
// ---------------------------------------------------------------------------
void vTask_VCU_Logic(void *pvParameters) {
  esp_task_wdt_add(nullptr);

  uint32_t lastStackLog = 0;

  while (true) {
    esp_task_wdt_reset();

    VcuLogic::run();

    logStackUsage("VCU_Task", lastStackLog);
    vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz loop
  }
}

// ---------------------------------------------------------------------------
// HMI task — updates display with current state and sensor data
// ---------------------------------------------------------------------------
void vTask_HMI_Display(void *pvParameters) {
  esp_task_wdt_add(nullptr);

  uint32_t lastStackLog = 0;

  DisplayHMI HMI_display;
  HMI_display.begin();

  uint16_t HMI_currentSpeed = 0;
  uint8_t HMI_currentBattery = 100;
  uint8_t HMI_incomingCommand = 0;

  while (true) {
    esp_task_wdt_reset();

    if (sensorDataQueue != nullptr) {
        xQueueReceive(sensorDataQueue, &HMI_currentSpeed, 0);
    }

    HMI_display.updateScreen(HMI_currentSpeed, HMI_currentBattery);

    if (HMI_display.readTouchCommand(HMI_incomingCommand)) {
        switch (HMI_incomingCommand) {
            case 1:
                ESP_LOGI(TAG, "HMI command: START request");
                VcuLogic::postEvent(VcuLogic::VcuEvent::START_REQUEST);
                break;
            case 2:
                ESP_LOGI(TAG, "HMI command: RESET request");
                VcuLogic::postEvent(VcuLogic::VcuEvent::RESET);
                break;
            case 3:
                ESP_LOGE(TAG, "HMI command: EMERGENCY STOP triggered");
                VcuLogic::postEvent(VcuLogic::VcuEvent::EMERGENCY_STOP);
                break;
            default:
                ESP_LOGW(TAG, "Unknown HMI command received: %d", HMI_incomingCommand);
                break;
        }
    }

    logStackUsage("HMI_Task", lastStackLog);
    vTaskDelay(pdMS_TO_TICKS(100)); // 10Hz refresh
  }
}

// ---------------------------------------------------------------------------
// LoRa UART task — reads UKS commands and posts events to VcuLogic
// ---------------------------------------------------------------------------
void vTask_LoRa_UKS(void *pvParameters) {
  esp_task_wdt_add(nullptr);

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
  uint32_t lastStackLog = 0;

  while (true) {
    esp_task_wdt_reset();

    int len =
        uart_read_bytes(LORA_UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(100));
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

    logStackUsage("LoRa_Task", lastStackLog);
  }
}

// ---------------------------------------------------------------------------
// Main application entry point
// ---------------------------------------------------------------------------
extern "C" void app_main() {
  // --- Hardware initialization (before any tasks) ---

  // 1. Initialize relay hardware (SPI + MCP23S17)
  if (!RelayManager::instance().begin()) {
    ESP_LOGE(TAG, "RelayManager init failed — HALTING");
    return;
  }

  // 2. Initialize VCU state machine (event queue, safety allOff, INIT→IDLE)
  VcuLogic::init();

  // 3. Create sensor data queue for inter-task communication
  sensorDataQueue = xQueueCreate(1, sizeof(uint16_t));
  if (sensorDataQueue == nullptr) {
    ESP_LOGE(TAG, "Failed to create sensorDataQueue");
    return;
  }

  ESP_LOGI(TAG, "All subsystems initialized — starting tasks");

  // --- FreeRTOS task creation ---
  xTaskCreatePinnedToCore(vTask_CAN_Comm, "CAN_Task", 4096, nullptr, 5, nullptr,
                          0);
  xTaskCreatePinnedToCore(vTask_HMI_Display, "HMI_Task", 4096, nullptr, 2,
                          nullptr, 0);
  xTaskCreatePinnedToCore(vTask_VCU_Logic, "VCU_Task", 4096, nullptr, 10,
                          nullptr, 1);
  xTaskCreatePinnedToCore(vTask_LoRa_UKS, "LoRa_Task", 3072, nullptr, 8,
                          nullptr, 0);
}