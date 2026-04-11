#include "VcuLogic.h"
#include "RelayManager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static constexpr const char* TAG = "VCU_LOGIC";

// ---------------------------------------------------------------------------
// Pre-charge timeout — adjust once hardware is decided
// ---------------------------------------------------------------------------
static constexpr uint32_t PRE_CHARGE_TIMEOUT_MS = 3000;
static constexpr uint32_t TASK_PERIOD_MS = 20;

namespace VcuLogic {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static VcuState s_state = VcuState::INIT;
static QueueHandle_t s_eventQueue = nullptr;
static uint32_t s_stateTimer = 0;  // ms spent in current state

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void handleInit();
static void handleIdle();
static void handlePreCharge();
static void handleReady();
static void handleDrive();
static void handleEmergencyStop();
static void handleFault();
static void transitionTo(VcuState next);
static bool pollEvent(VcuEvent& out);

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void init() {
    s_eventQueue = xQueueCreate(8, sizeof(VcuEvent));
    if (s_eventQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
    }
    transitionTo(VcuState::IDLE);
}

void run() {
    s_stateTimer += TASK_PERIOD_MS;

    // Emergency stop has highest priority — check regardless of state
    VcuEvent event = VcuEvent::NONE;
    if (pollEvent(event)) {
        if (event == VcuEvent::EMERGENCY_STOP) {
            transitionTo(VcuState::EMERGENCY_STOP);
            return;
        }
        if (event == VcuEvent::FAULT_DETECTED) {
            transitionTo(VcuState::FAULT);
            return;
        }
        if (event == VcuEvent::RESET && s_state == VcuState::FAULT) {
            transitionTo(VcuState::IDLE);
            return;
        }
    }

    switch (s_state) {
        case VcuState::INIT:
            handleInit();
            break;
        case VcuState::IDLE:
            handleIdle();
            break;
        case VcuState::PRE_CHARGE:
            handlePreCharge();
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
    }
}

void postEvent(VcuEvent event) {
    if (s_eventQueue == nullptr)
        return;
    // Non-blocking — if queue is full, event is dropped (logged below)
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
static void handleInit() {
    // Shouldn't stay here — init() moves us to IDLE immediately
    transitionTo(VcuState::IDLE);
}

static void handleIdle() {
    VcuEvent event = VcuEvent::NONE;
    // Event already consumed above — check again for START_REQUEST
    // (pollEvent is called once per run(), so we use the already-polled event)
    // In IDLE, we wait for a start request to begin pre-charge
    // Nothing to do actively — system is safe, all contactors open
}

static void handlePreCharge() {
    // TODO: Replace timer with actual voltage threshold check
    // e.g. if (readBusVoltage() >= PACK_VOLTAGE * 0.95f) → PRE_CHARGE_DONE

    if (s_stateTimer >= PRE_CHARGE_TIMEOUT_MS) {
        ESP_LOGI(TAG, "Pre-charge complete (timer-based placeholder)");
        postEvent(VcuEvent::PRE_CHARGE_DONE);
    }

    // TODO: Add voltage runaway detection here
    // if (busVoltageRising() == false after 1s) → FAULT
}

static void handleReady() {
    // HV bus is live — waiting for drive enable
    // TODO: Monitor bus voltage, temperature (when sensors are wired)
}

static void handleDrive() {
    // Active driving state
    // TODO: Forward torque requests from CAN task to motor controller
    // TODO: Monitor current limits, thermal limits
}

static void handleEmergencyStop() {
    // TODO: Open all contactors via RelayManager
    // RelayManager::instance().setOutput(CONTACTOR_MAIN, false);
    // RelayManager::instance().setOutput(CONTACTOR_PRE, false);

    RelayManager::instance().allOff();  // ← bunu ekle
    ESP_LOGE(TAG, "EMERGENCY STOP — all relays de-energized");
}

static void handleFault() {
    // Safe state — all outputs should already be de-energized
    // Log once per second to avoid flooding
    static uint32_t lastLogTimer = 0;
    if (s_stateTimer - lastLogTimer >= 1000) {
        ESP_LOGE(TAG, "System in FAULT state. Send RESET event to recover.");
        lastLogTimer = s_stateTimer;
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