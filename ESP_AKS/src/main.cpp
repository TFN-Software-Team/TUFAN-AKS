#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "DisplayHMI.h"
#include "CanManager.h"
#include "RelayManager.h"
#include "SystemConfig.h"
#include "Telemetry.h"
#include "VcuLogic.h"

static constexpr const char *TAG = "APP_MAIN";

// Stack high-water-mark logging interval (ticks)
static constexpr uint32_t STACK_LOG_INTERVAL_MS = 30000;

QueueHandle_t TEL_sensorDataQueue = nullptr;

static void CAN_handleEvent(CAN_Event CAN_event, void* CAN_context) {
  (void)CAN_context;

  switch (CAN_event) {
  case CAN_Event::FAULT_DETECTED:
    ESP_LOGE(TAG, "CAN fault event received");
    VcuLogic::postEvent(VcuLogic::VcuEvent::FAULT_DETECTED);
    break;
  default:
    break;
  }
}

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
  can.setEventCallback(CAN_handleEvent, nullptr);

  if (!can.begin()) {
    ESP_LOGE(TAG, "Failed to initialize CAN bus");
    esp_task_wdt_delete(nullptr);
    vTaskDelete(nullptr);
    return;
  }

  // Phase 1.2 placeholder:
  // keep propulsion torque at zero until the pedal / brake conversion model
  // is defined and validated with vehicle controls.
  uint16_t CAN_torqueCmd = 0;
  uint32_t lastStackLog = 0;

  while (true) {
    esp_task_wdt_reset();

    // 1. Read incoming messages and dispatch them
    can.processRxMessages();

    // 2. Send torque command if in DRIVE state
    if (VcuLogic::getState() == VcuLogic::VcuState::DRIVE) {
      // TODO: Get actual torque value from control logic
      can.sendTorqueCommand(CAN_torqueCmd);
    } else {
      // Not in drive — send zero torque for safety
      can.sendTorqueCommand(0);
    }

    // 3. Push the latest telemetry snapshot to the shared queue
    if (TEL_sensorDataQueue != nullptr) {
      const TelemetryData TEL_data = can.getTelemetryData();
      xQueueOverwrite(TEL_sensorDataQueue, &TEL_data);
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
  if (!HMI_display.begin()) {
    ESP_LOGE(TAG, "DisplayHMI init failed — HMI task terminating");
    esp_task_wdt_delete(nullptr);
    vTaskDelete(nullptr);
    return;
  }

  uint16_t HMI_currentSpeed = 0;
  uint8_t HMI_currentBattery = 100;
  uint8_t HMI_incomingCommand = 0;

  while (true) {
    esp_task_wdt_reset();

    if (TEL_sensorDataQueue != nullptr) {
        TelemetryData TEL_data = {};
        if (xQueuePeek(TEL_sensorDataQueue, &TEL_data, 0) == pdTRUE) {
            HMI_currentSpeed = TEL_data.TEL_motorRpm;
            HMI_currentBattery = TEL_data.TEL_bmsSoc;
        }
    }

    HMI_display.updateScreen(HMI_currentSpeed, HMI_currentBattery);

    if (HMI_display.readTouchCommand(HMI_incomingCommand)) {
        switch (HMI_incomingCommand) {
            case HMI_CMD_START:
                ESP_LOGI(TAG, "HMI command: START request");
                VcuLogic::postEvent(VcuLogic::VcuEvent::START_REQUEST);
                break;
            case HMI_CMD_RESET:
                ESP_LOGI(TAG, "HMI command: RESET request");
                VcuLogic::postEvent(VcuLogic::VcuEvent::RESET);
                break;
            case HMI_CMD_EMERGENCY_STOP:
                ESP_LOGE(TAG, "HMI command: EMERGENCY STOP triggered");
                VcuLogic::postEvent(VcuLogic::VcuEvent::EMERGENCY_STOP);
                break;
            case HMI_CMD_DRIVE_ENABLE:
                ESP_LOGI(TAG, "HMI command: DRIVE ENABLE request");
                VcuLogic::postEvent(VcuLogic::VcuEvent::DRIVE_ENABLE);
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

  Telemetry LO_telemetry;

  // Planned E32 startup sequence:
  // 1. Drive M0/M1 to the normal-mode levels from SystemConfig.h.
  // 2. Configure AUX as input and wait for ready before TX.
  // 3. Keep UART traffic in transparent mode unless configuration mode is
  //    explicitly needed later.

  // UART init for E32
  uart_config_t LO_uartConfig = {
      .baud_rate = LORA_UART_BAUD,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };
  uart_param_config(LORA_UART_NUM, &LO_uartConfig);
  uart_set_pin(LORA_UART_NUM, LORA_TX_PIN, LORA_RX_PIN, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
  uart_driver_install(LORA_UART_NUM, 256, 0, 0, nullptr, 0);
  LO_telemetry.begin();

  uint8_t LO_rxBuffer[4];
  uint32_t lastStackLog = 0;
  TickType_t LO_lastTelemetryTick = 0;

  while (true) {
    esp_task_wdt_reset();

    int LO_rxLength = uart_read_bytes(LORA_UART_NUM, LO_rxBuffer,
                                      sizeof(LO_rxBuffer),
                                      pdMS_TO_TICKS(100));
    if (LO_rxLength > 0) {
      switch (LO_rxBuffer[0]) {
      case UKS_CMD_EMERGENCY_STOP:
        VcuLogic::postEvent(VcuLogic::VcuEvent::EMERGENCY_STOP);
        break;
      case UKS_CMD_START:
        VcuLogic::postEvent(VcuLogic::VcuEvent::START_REQUEST);
        break;
      case UKS_CMD_STOP:
        VcuLogic::postEvent(VcuLogic::VcuEvent::RESET);
        break;
      case UKS_CMD_DRIVE_ENABLE:
        ESP_LOGI(TAG, "LoRa command: DRIVE ENABLE request");
        VcuLogic::postEvent(VcuLogic::VcuEvent::DRIVE_ENABLE);
        break;
      default:
        break;
      }
    }

    const TickType_t LO_nowTick = xTaskGetTickCount();
    if ((LO_nowTick - LO_lastTelemetryTick) >=
        pdMS_TO_TICKS(LORA_TX_PERIOD_MS)) {
      if (TEL_sensorDataQueue != nullptr) {
        TelemetryData TEL_data = {};
        if (xQueuePeek(TEL_sensorDataQueue, &TEL_data, 0) == pdTRUE) {
          LO_telemetry.sendStatus(TEL_data);
          LO_lastTelemetryTick = LO_nowTick;
        }
      }
    }

    logStackUsage("LoRa_Task", lastStackLog);
    vTaskDelay(pdMS_TO_TICKS(10));
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
  TEL_sensorDataQueue = xQueueCreate(1, sizeof(TelemetryData));
  if (TEL_sensorDataQueue == nullptr) {
    ESP_LOGE(TAG, "Failed to create TEL_sensorDataQueue");
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
