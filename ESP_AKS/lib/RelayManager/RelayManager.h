#pragma once
#include <cstdint>
#include "driver/spi_master.h"

#define MCP23S17_IODIRA 0x00
#define MCP23S17_IODIRB 0x01
#define MCP23S17_OLATA 0x14
#define MCP23S17_OLATB 0x15
#define MCP23S17_ADDR 0x40

class RelayManager {
   public:
    static RelayManager& instance();

    bool begin();
    void setRelay(uint8_t channel, bool state);
    void allOn();   // Close all 10 positive contactors
    void allOff();  // Open all — SAFETY

    // Read back current relay state for diagnostics
    bool getRelayState(uint8_t channel) const;

   private:
    RelayManager() = default;
    void writeRegister(uint8_t reg, uint8_t value);

    uint16_t s_relayState = 0;
    spi_device_handle_t s_spiDev = nullptr;
    bool s_initialized = false;
};