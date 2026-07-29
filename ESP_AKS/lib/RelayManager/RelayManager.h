#pragma once
#include "SystemConfig.h"
#include <atomic>
#include <cstdint>
#include "driver/spi_master.h"
#include "driver/gpio.h"

// --- MCP23S17 register adresleri (BANK=0 haritası) ---
// DİKKAT: bu adresler yalnızca IOCON.BANK=0 iken geçerlidir. BANK=1'de tüm
// harita kayar (ör. OLATA 0x14 → 0x0A) ve aşağıdaki sabitler YANLIŞ register'a
// yazar. Bu yüzden begin()/reinitAndReassert() İLK İŞ olarak IOCON'u bilinen
// bir değere (BANK=0) çeker — bkz. MCP23S17_IOCON_* notu.
#define MCP23S17_IODIRA 0x00
#define MCP23S17_IODIRB 0x01
#define MCP23S17_OLATA 0x14
#define MCP23S17_OLATB 0x15

// IOCON'un İKİ adresi vardır ve hangisinin geçerli olduğu IOCON.BANK bitinin
// KENDİSİNE bağlıdır (tavuk-yumurta): BANK=0 iken 0x0A, BANK=1 iken 0x05.
// Chip'in hangi modda olduğunu BİLMEDİĞİMİZ için (brown-out, kısmi reset,
// gürültülü SPI, veya önceki bir firmware) HER İKİSİNE de 0x00 yazılır:
//   * Chip BANK=1'deyse  → 0x05'e yazım IOCON'u bulur ve BANK=0'a çeker.
//   * Chip BANK=0'daysa  → 0x0A'ya yazım IOCON'u bulur (0x05 yazımı zararsız
//                          bir GPINTENB/DEFVAL bölgesine düşer, hepsi 0x00).
// Sıra ÖNEMLİ: önce 0x05 (BANK=1 varsayımı), sonra 0x0A (BANK=0 teyidi).
// Y31: bu yazım olmadan, chip beklenmedik bir sebeple BANK=1'e düşerse tüm
// röle yazımları sessizce yanlış register'lara gider — kontaktörler komut
// edildiği gibi davranmaz ve geri-okuma doğrulaması da yanıltıcı olur.
#define MCP23S17_IOCON_BANK0 0x0A
#define MCP23S17_IOCON_BANK1 0x05

#define MCP23S17_ADDR 0x40       // SPI opcode, R/W=0 (write)
#define MCP23S17_ADDR_READ 0x41  // SPI opcode, R/W=1 (read) — G3 geri-okuma yolu

// ---------------------------------------------------------------------------
// R3 — İş parçacığı (task) güvenliği sözleşmesi:
//   * s_relayState'e YALNIZCA VCU task'i yazar (setRelay/allOn/allOff üzerinden).
//     Röle yazımları + SPI transaction'ları zaten VCU tick'inde serileşir, o
//     yüzden yazma tarafında mutex GEREKMEZ (tek yazar, tek word RMW).
//   * HMI task'i durumu YALNIZCA okur (getRelayState) ve BAYAT okuma kabul
//     edilir (bir tick gecikme diagnostikte önemsiz).
// Okuma tarafını (HMI) yırtılmış okumaya karşı korumak için s_relayState tek
// word (uint16_t) olduğundan std::atomic yapıldı — mutex'e gerek yok. relaxed
// order yeterli: bayrak başka veri publish etmiyor, yalnız kendi görünürlüğü.
// s_actuatorFault de aynı gerekçeyle zaten atomic (VCU/verify yazar, VcuLogic okur).
class RelayManager {
   public:
    static RelayManager& instance();

    bool begin();
    void setRelay(uint8_t channel, bool state);
    // allOn/allOff KONTAKTÖR BANK maskesini (RELAY_CONTACTOR_BANK_MASK,
    // SystemConfig.h) sürer. RELAY_ROLES_ASSIGNED=0 iken maske 10 kanalın
    // tamamıdır (eski davranışla birebir aynı); roller atandığında flaşör
    // kanalı maskenin dışındadır ve son yazılan durumu shadow'da korunur.
    void allOn();   // Close contactor bank (mask)

    // mask'taki 1 olan kanalları TEK TEK, aralarında stepDelayMs bekleyerek enerjilendirir.
    void setBankStaggered(uint16_t mask, uint32_t stepDelayMs = RELAY_STAGGER_STEP_MS);

    void allOff(bool silent = false);  // Open contactor bank — SAFETY (8.2.a.vi)

    // Read back current relay state for diagnostics
    bool getRelayState(uint8_t channel) const;

    // --- G3: MCP23S17 geri-okuma / çıkış doğrulama yolu ---
    // Tek bir register'ı SPI read (0x41) ile okur. Başarıda true ve out set.
    bool readRegister(uint8_t reg, uint8_t& out);

    // OLATA/OLATB/IODIRA/IODIRB'i geri okur, beklenen gölge-durumla (shadow:
    // ~s_relayState + tüm pinler output) karşılaştırır. Uyuşmazlıkta chip'i
    // yeniden init edip çıkışları re-assert eder ve actuator fault bayrağını
    // KALICI olarak set eder (VcuLogic her tick okur). Çıkışlar eşleşiyorsa
    // true döner. begin()/allOn()/allOff()/setRelay() sonrası HEMEN çağrılır.
    bool verifyOutputs();

    // VCU task tick'inden çağrılır; RELAY_VERIFY_PERIOD_MS'den seyrek
    // olmayacak şekilde verifyOutputs()'u tetikler (her tick değil).
    void verifyIfDue(uint32_t nowMs);

    // VcuLogic'in her tick okuyabileceği kalıcı atomic actuator-fault bayrağı
    // (R1: kuyruğa/olaya güvenilmez — düşen event tuzağı yok).
    bool hasActuatorFault() const;
    void clearActuatorFault();  // VcuLogic RESET yolunda çağrılır

#ifdef NATIVE_BUILD
    // Yalnız native test build'inde aktif. Singleton'ın iç state'ini
    // sıfırlar (relayState, init flag, SPI handle). Production build'inde
    // tanımlı değildir.
    void resetForTest();
#endif

    // MANTIKSAL kanal durumu -> MCP23S17 pin seviyeleri. TEK dönüşüm noktası:
    // begin(), setRelay(), allOn(), allOff(), reinitAndReassert() ve
    // verifyOutputs() İSTİSNASIZ bunu kullanır, böylece yazılan ve beklenen
    // (geri-okuma) desen yapısal olarak aynı kalır.
    //   * Varsayılan (NO klemens): active-low sürücü katı → pin = !mantıksal.
    //   * RELAY_INVERT_MASK'taki kanal (NC klemens): pin = mantıksal — röleyi
    //     enerjisiz bırakmak yükü ÇALIŞTIRDIĞI için mantık terslenir
    //     (bkz. SystemConfig.h "KANAL BAZINDA POLARITE", SORUN 2 / 2026-07-29).
    // Kullanılmayan üst bitler (>= RELAY_TOTAL_CHANNELS) 1 kalır — o pinlerde
    // yük yoktur, eski davranışla aynı.
    static constexpr uint16_t hwFromLogical(uint16_t logical) {
        return (uint16_t)(~logical ^ (uint16_t)RELAY_INVERT_MASK);
    }

   private:
    RelayManager() = default;
    void writeRegister(uint8_t reg, uint8_t value);
    // Güvenli init sırasını (OLAT HIGH → IODIR output) tekrar uygular ve
    // s_relayState'i re-assert eder. verifyOutputs() uyuşmazlıkta çağırır.
    void reinitAndReassert();

    // R3: tek word — HMI okuma tarafı torn-read'e karşı atomic (yukarıdaki
    // sözleşmeye bakınız). Yalnız VCU task yazar; relaxed order yeterli.
    std::atomic<uint16_t> s_relayState{0};
    spi_device_handle_t s_spiDev = nullptr;
    bool s_initialized = false;

    std::atomic<bool> s_actuatorFault{false};
    uint32_t s_lastVerifyMs = 0;
    bool s_verifyStarted = false;  // ilk verifyIfDue çağrısında periyodu başlat
};
