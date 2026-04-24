#include "VcuLogic.h"
#include "RelayManager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static constexpr const char* TAG = "VCU_LOGIC";
static constexpr uint32_t TASK_PERIOD_MS = 20;

namespace VcuLogic {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static VcuState s_state = VcuState::INIT;
static QueueHandle_t s_eventQueue = nullptr;
static uint32_t s_stateTimer = 0;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void transitionTo(VcuState next);
static bool pollEvent(VcuEvent& out);

static void handleIdle();
static void handleReady();
static void handleDrive();
static void handleEmergencyStop();
static void handleFault();

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void init() {
    s_eventQueue = xQueueCreate(8, sizeof(VcuEvent));
    if (s_eventQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }

    // Safety first — ensure all relays are off at startup
    RelayManager::instance().allOff();

    transitionTo(VcuState::IDLE);
}

void run() {
    s_stateTimer += TASK_PERIOD_MS;

    VcuEvent event = VcuEvent::NONE;
    if (pollEvent(event)) {
        // High priority events — handled regardless of current state
        if (event == VcuEvent::EMERGENCY_STOP) {
            transitionTo(VcuState::EMERGENCY_STOP);
            return;
        }
        if (event == VcuEvent::FAULT_DETECTED) {
            transitionTo(VcuState::FAULT);
            return;
        }
        if (event == VcuEvent::RESET &&
            (s_state == VcuState::FAULT ||
             s_state == VcuState::EMERGENCY_STOP)) {
            transitionTo(VcuState::IDLE);
            return;
        }

        // State-specific event handling
        switch (s_state) {
            case VcuState::IDLE:
                if (event == VcuEvent::START_REQUEST)
                    transitionTo(VcuState::READY);
                break;

            case VcuState::READY:
                if (event == VcuEvent::DRIVE_ENABLE)
                    transitionTo(VcuState::DRIVE);
                break;

            default:
                break;
        }
    }

    // Periodic state logic
    switch (s_state) {
        case VcuState::IDLE:
            handleIdle();
            break;
        case VcuState::READY:
            handleReady();
            break;
        case VcuState::DRIVE:
            handleDrive();
            break;
        case VcuState::EMERGENCY_STOP:
            handleEmergencyStop();
            break;
        case VcuState::FAULT:
            handleFault();
            break;
        default:
            break;
    }
}

void postEvent(VcuEvent event) {
    if (s_eventQueue == nullptr)
        return;
    if (xQueueSend(s_eventQueue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropped event %d",
                 static_cast<int>(event));
    }
}

VcuState getState() {
    return s_state;
}

// ---------------------------------------------------------------------------
// State handlers
// ---------------------------------------------------------------------------
static void handleIdle() {
    // All relays off — safe resting state
    // Waiting for START_REQUEST from LoRa/UKS
}

static void handleReady() {
    // Close all positive contactors on entry (runs once via stateTimer guard)
    if (s_stateTimer <= TASK_PERIOD_MS) {
        RelayManager::instance().allOn();
        ESP_LOGI(TAG, "All contactors closed — system READY");
    }
    // DRIVE is entered only after an explicit DRIVE_ENABLE command.
    // Future interlocks should be added here before propulsion is allowed.
}

static void handleDrive() {
    // Contactors remain closed during drive
    // Torque output is still held at zero in the CAN task until the Phase 1.2
    // input-to-torque model is implemented.
}

static void handleEmergencyStop() {
    // Phase 1.3 note:
    // contactors are still opened immediately. Replace this with a coordinated
    // zero-torque -> hold -> open sequence once torque shutdown handoff exists.
    RelayManager::instance().allOff();

    // Log once per second to avoid flooding
    static uint32_t lastLog = 0;
    if (s_stateTimer - lastLog >= 1000) {
        ESP_LOGE(TAG, "EMERGENCY STOP active — all relays off");
        lastLog = s_stateTimer;
    }
    // Recovery only via physical reset or explicit RESET event
}

static void handleFault() {
    // Phase 1.3 note:
    // apply the same coordinated shutdown sequence here when the motor torque
    // decay timing is defined.
    RelayManager::instance().allOff();

    static uint32_t lastLog = 0;
    if (s_stateTimer - lastLog >= 1000) {
        ESP_LOGE(TAG, "FAULT state — send RESET event to recover");
        lastLog = s_stateTimer;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void transitionTo(VcuState next) {
    ESP_LOGI(TAG, "State: %d → %d", static_cast<int>(s_state),
             static_cast<int>(next));
    s_state = next;
    s_stateTimer = 0;
}

static bool pollEvent(VcuEvent& out) {
    if (s_eventQueue == nullptr)
        return false;
    return xQueueReceive(s_eventQueue, &out, 0) == pdTRUE;
}

}  // namespace VcuLogic
