#pragma once
#include <cstdint>

class DisplayHMI {
   private:
    bool HMI_isInitialized;
    void HMI_sendEndBytes();

   public:
    DisplayHMI();
    bool begin();
    void updateScreen(uint8_t HMI_speed, uint8_t HMI_battery);
    bool readTouchCommand(uint8_t& HMI_command);
};