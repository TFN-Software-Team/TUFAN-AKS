// M2: VcuLogic'e enjekte edilen IRelayActuator mock'u. Gerçek SPI/RelayManager
// yok; çağrılar global sayaçlara işlenir, testler bunları doğrular.
#include "mock_relay_actuator.h"

// "Bank kapatma yolu cagrildi" sayaci — allOn() VE setBankStaggered() ikisini
// de sayar. Testlerin cogu "READY girisinde bank kapandi mi?" diye sorar ve
// hangi yolun kullanildigi onlar icin fark etmez.
unsigned g_fake_relay_allOn_count = 0;
// YALNIZ allOn() sayaci. RELAY_ROLES_ASSIGNED=1'de READY girisi allOn()
// KULLANMAMALIDIR: allOn() TUM bank maskesini (S1 dahil) kapatir ve sarj
// hattini surus sirasinda enerjilendirirdi (sartname 8.2.a.vii ihlali).
// Roles suite'i bu sayacin 0 kalmasini dogrular; g_fake_relay_allOn_count
// bunu AYIRT EDEMEZ (setBankStaggered ile ortaktir).
unsigned g_fake_relay_allOnDirect_count = 0;
unsigned g_fake_relay_allOff_count = 0;
unsigned g_fake_relay_allOff_silent_count = 0;
unsigned g_fake_relay_setRelay_count = 0;

bool     g_fake_relay_actuatorFault = false;
unsigned g_fake_relay_clearFault_count = 0;
unsigned g_fake_relay_verifyIfDue_count = 0;

unsigned g_fake_call_seq = 0;
unsigned g_fake_relay_allOff_firstSeq = 0;

bool g_fake_relay_channelState[RELAY_TOTAL_CHANNELS] = {};

MockRelayActuator g_mockRelay;

void fake_relay_reset(void) {
    g_fake_relay_allOn_count = 0;
    g_fake_relay_allOnDirect_count = 0;
    g_fake_relay_allOff_count = 0;
    g_fake_relay_allOff_silent_count = 0;
    g_fake_relay_setRelay_count = 0;
    g_fake_relay_actuatorFault = false;
    g_fake_relay_clearFault_count = 0;
    g_fake_relay_verifyIfDue_count = 0;
    g_fake_call_seq = 0;
    g_fake_relay_allOff_firstSeq = 0;
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch)
        g_fake_relay_channelState[ch] = false;
}

// allOn/allOff gerçek RelayManager ile aynı BANK-MASKESİ semantiğini taklit
// eder: yalnız RELAY_CONTACTOR_BANK_MASK içindeki kanallar yazılır; maske
// dışındaki kanalın (roller atandığında flaşör) durumu KORUNUR.
void MockRelayActuator::allOn() {
    ++g_fake_relay_allOn_count;
    ++g_fake_relay_allOnDirect_count;
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch))
            g_fake_relay_channelState[ch] = true;
    }
}

void MockRelayActuator::setBankStaggered(uint16_t mask, uint32_t /*stepDelayMs*/) {
    ++g_fake_relay_allOn_count;
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (mask & (1u << ch))
            g_fake_relay_channelState[ch] = true;
    }
}

void MockRelayActuator::allOff(bool silent) {
    ++g_fake_relay_allOff_count;
    if (g_fake_relay_allOff_firstSeq == 0)
        g_fake_relay_allOff_firstSeq = ++g_fake_call_seq;  // G2: sıra kaydı
    if (silent)
        ++g_fake_relay_allOff_silent_count;
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch))
            g_fake_relay_channelState[ch] = false;
    }
}

void MockRelayActuator::setRelay(uint8_t channel, bool state) {
    ++g_fake_relay_setRelay_count;
    if (channel < RELAY_TOTAL_CHANNELS)
        g_fake_relay_channelState[channel] = state;
}

// --- G3: geri-okuma / actuator fault yolu (mock) ---
// Gerçek SPI geri-okuma yok; fault durumu testler tarafından
// g_fake_relay_actuatorFault ile enjekte edilir. VcuLogic bunları her tick
// okur (verifyIfDue + hasActuatorFault).
void MockRelayActuator::verifyIfDue(uint32_t /*nowMs*/) {
    ++g_fake_relay_verifyIfDue_count;
}

bool MockRelayActuator::verifyOutputs() {
    // Mock: her zaman "çıkışlar eşleşiyor" döner; fault enjeksiyonu ayrı bayrak
    // (g_fake_relay_actuatorFault) üzerinden yapılır.
    return !g_fake_relay_actuatorFault;
}

bool MockRelayActuator::hasActuatorFault() const {
    return g_fake_relay_actuatorFault;
}

void MockRelayActuator::clearActuatorFault() {
    g_fake_relay_actuatorFault = false;
    ++g_fake_relay_clearFault_count;
}
