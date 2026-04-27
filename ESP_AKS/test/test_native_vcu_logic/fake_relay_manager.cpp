// Native testler için RelayManager şeyi: SPI çağrılarının yerine basit state
// ve sayaç tutar. Testler `g_fake_relay_*_count` üzerinden VcuLogic'in
// kontaktör isteklerini doğrulayabilir.
#include "RelayManager.h"

unsigned g_fake_relay_allOn_count = 0;
unsigned g_fake_relay_allOff_count = 0;
unsigned g_fake_relay_setRelay_count = 0;

void fake_relay_reset(void) {
    g_fake_relay_allOn_count = 0;
    g_fake_relay_allOff_count = 0;
    g_fake_relay_setRelay_count = 0;
}

RelayManager& RelayManager::instance() {
    static RelayManager inst;
    return inst;
}

bool RelayManager::begin() {
    s_initialized = true;
    s_relayState = 0;
    return true;
}

void RelayManager::setRelay(uint8_t channel, bool state) {
    if (channel >= 16)
        return;
    if (state)
        s_relayState |= (1u << channel);
    else
        s_relayState &= ~(1u << channel);
    ++g_fake_relay_setRelay_count;
}

void RelayManager::allOn() {
    s_relayState = 0x03FF;  // 10 kanal
    ++g_fake_relay_allOn_count;
}

void RelayManager::allOff() {
    s_relayState = 0;
    ++g_fake_relay_allOff_count;
}

bool RelayManager::getRelayState(uint8_t channel) const {
    if (channel >= 16)
        return false;
    return (s_relayState >> channel) & 0x1;
}

void RelayManager::writeRegister(uint8_t /*reg*/, uint8_t /*value*/) {
    // no-op
}

void RelayManager::resetForTest() {
    s_relayState = 0;
    s_initialized = false;
    s_spiDev = nullptr;
}
