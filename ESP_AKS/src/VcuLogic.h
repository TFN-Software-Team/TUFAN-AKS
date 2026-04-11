#pragma once
#include <cstdint>

namespace VcuLogic {

// ---------------------------------------------------------------------------
// State definitions
// ---------------------------------------------------------------------------
enum class VcuState : uint8_t {
    INIT = 0,
    IDLE = 1,
    PRE_CHARGE = 2,  // TODO: implement when contactor HW is decided
    READY = 3,       // HV bus ready, waiting for drive command
    DRIVE = 4,
    EMERGENCY_STOP = 5,  // Triggered by UKS via LoRa/SPI
    FAULT = 6            // Unrecoverable error — requires reset
};

// ---------------------------------------------------------------------------
// Events that can trigger state transitions
// ---------------------------------------------------------------------------
enum class VcuEvent : uint8_t {
    NONE = 0,
    START_REQUEST = 1,    // Normal start (e.g. from HMI or CAN)
    PRE_CHARGE_DONE = 2,  // TODO: tie to voltage sensor threshold
    DRIVE_ENABLE = 3,     // Drive command received
    EMERGENCY_STOP = 4,   // From UKS via LoRa
    FAULT_DETECTED = 5,   // Any critical fault
    RESET = 6             // Manual reset from FAULT state
};

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------
void init();
void run();                      // Called every 20ms from FreeRTOS task
void postEvent(VcuEvent event);  // Thread-safe event injection
VcuState getState();

}  // namespace VcuLogic