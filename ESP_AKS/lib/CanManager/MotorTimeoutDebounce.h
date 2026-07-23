#pragma once

#include <stdint.h>

// MotorTimeoutDebounce.h
// Saf mantik: N ardisik timeout periyodu boyunca frame alinmazsa
// debounce edilmis timeout kararini verir.

inline bool motor_timeout_debounce_evaluate(uint8_t& debounceCount, uint32_t nowTick, uint32_t& lastCheckTick, uint32_t lastStatusTick, uint32_t timeoutTicks, uint8_t debounceThreshold) {
    // Sadece her timeoutTicks periyodunda bir degerlendirme yap (rate limiting)
    if (nowTick - lastCheckTick >= timeoutTicks) {
        lastCheckTick = nowTick;
        
        // Eger son alinan frame'den bu yana timeoutTicks kadar sure gectiyse
        if (nowTick - lastStatusTick >= timeoutTicks) {
            debounceCount++;
        } else {
            debounceCount = 0;
        }
    }
    
    return debounceCount >= debounceThreshold;
}
