#pragma once
#include <cstdint>
#include "driver/spi_master.h"

// MCP23S17 register addresses
#define MCP23S17_IODIRA 0x00
#define MCP23S17_IODIRB 0x01
#define MCP23S17_OLATA 0x14
#define MCP23S17_OLATB 0x15
#define MCP23S17_ADDR 0x40  // A2=A1=A0=0 on schematic

class RelayManager {
   public:
    static RelayManager& instance();

    bool begin();

    // channel: 0-9 (maps to OUT0-OUT9 on schematic)
    void setRelay(uint8_t channel, bool state);
    void allOff();  // Safety: de-energize all relays

   private:
    RelayManager() = default;

    void writeRegister(uint8_t reg, uint8_t value);
    uint16_t s_relayState = 0;  // bitmask for OUT0-OUT9
    spi_device_handle_t s_spiDev = nullptr;
    bool s_initialized = false;
};