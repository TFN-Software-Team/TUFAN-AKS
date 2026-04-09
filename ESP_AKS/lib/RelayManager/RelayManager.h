#pragma once
#include <cstdint>

class RelayManager {
   public:
    RelayManager();
    bool begin();
    void setOutput(uint8_t channel, bool state);
};
