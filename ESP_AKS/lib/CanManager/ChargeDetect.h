#pragma once
#include <cstdint>

#include "SystemConfig.h"  // CHARGE_DETECT_CURRENT_CENTI_A, _DEBOUNCE_SAMPLES, _MAX_RPM

// ---------------------------------------------------------------------------
// Akim tabanli sarj tespiti — SAF karar mantigi (Y20 gozlemi).
// ---------------------------------------------------------------------------
// NEDEN VAR: `TEL_chargerActive`in TEK kaynagidir (2026-07-29 oncesinde
// charger komut frame'inin (0x1806E5F4) tazeligi BIRINCIL kaynakti ve bununla
// OR'laniyordu). Iki canli CAN kaydi o frame'in sarj gostergesi OLMADIGINI
// kanitladi: BMS, charger'a hitaben setpoint yayinini sarjda olsun olmasin
// ~100 ms'de bir SUREKLI yapar (esp32-session.log: 4733 frame, payload
// `03 70 03 E8` degismeden; batarya_tam_kayit(1).log: 1598 frame, ayni
// periyot). Varligini "sarjda" saymak sistemi 7/24 yanlis pozitife sokuyordu.
// Bkz. SystemConfig.h CHARGE_DETECT_* blogu.
//
// GOSTERGE — ayni iki kayit, 0xE000 byte[0:1] (DOGRULANMIS alan):
//   sarjda degil  -0.1 / -0.2 A  (= -10 / -20 centi-A)
//   sarjda       +9.1 … +10.0 A  (= +910 … +1000 centi-A)
// Ekip PCAN olcumuyle (Y20, BENI_OKU.md 5.3) birebir ortusur; suruste -5.6 A.
//
// DORT KORUMA KATMANI:
//   1. ESIK    : yalniz POZITIF ve CHARGE_DETECT_CURRENT_CENTI_A ustu akim.
//                Desarj (negatif) ASLA sarj sayilmaz.
//   2. DEBOUNCE (acilis): CHARGE_DETECT_DEBOUNCE_SAMPLES ardisik ornek gerekir;
//                tek orneklik gurultu/tepe bayragi actirmaz.
//   3. RELEASE  (kapanis): CHARGE_DETECT_RELEASE_SAMPLES ardisik esik-alti
//                ornek gerekir (~2 sn). Akim artik TEK gosterge oldugundan sart:
//                sarj sonu CV/taper fazinda akim dogal olarak esigin altina inip
//                yeniden cikar; tek ornekte dusmek "SARJ OLUYOR" gostergesini ve
//                S1 kararlarini cirpindirirdi.
//   4. HAREKETSIZLIK: motor surucusu geldiginde REJENERATIF FRENLEME de
//                bataryaya POZITIF akim basar. Rejeni "sarj" sanmamak icin
//                arac fiilen DURUYOR olmalidir (|rpm| < CHARGE_DETECT_MAX_RPM).
//                Bu kapi release debounce'una TABI DEGILDIR — arac hareket
//                ettigi ANDA bayrak duser (guvenli taraf).
// Ayrica BMS verisi taze degilse (bmsDataValid=false) akim son gorulen bayat
// degeri tutar — o degerden karar URETILMEZ (esik-alti gibi islenir, yani
// release sayacini isletir; ~2 sn icinde veri donmezse bayrak duser).
//
// Durum (ardisik ornek sayaclari) cagirana aittir; ayni idiom:
// lib/VcuLogic/HeadlightSwitch.h, lib/DisplayHMI/ResyncPolicy.h.
namespace ChargeDetect {

struct State {
    uint8_t consecutiveAboveThreshold;   // ust uste esigi asan ornek sayisi
    uint16_t consecutiveBelowThreshold;  // ust uste esik altinda kalan ornek
    bool detected;                       // debounce sonrasi karar (cikti)
};

inline State makeState() { return State{0, 0, false}; }

// Parametreli (test edilebilir) cekirdek. Uretim kodu asagidaki update()
// sarmalayicisini kullanir (SysStateDerive::deriveFromCurrentImpl ile ayni desen).
inline bool updateImpl(State& st, int32_t bmsCurrentCentiA, bool bmsDataValid,
                       int16_t motorRpm, int32_t thresholdCentiA,
                       uint8_t debounceSamples, int16_t maxRpm,
                       uint16_t releaseSamples) {
    // (4) HAREKET KAPISI — release debounce'undan MUAF, aninda duser.
    const bool stationary = (motorRpm < maxRpm) && (motorRpm > -maxRpm);
    if (!stationary) {
        st.consecutiveAboveThreshold = 0;
        st.consecutiveBelowThreshold = 0;
        st.detected = false;
        return st.detected;
    }

    const bool aboveThreshold =
        bmsDataValid && (bmsCurrentCentiA > thresholdCentiA);

    if (aboveThreshold) {
        st.consecutiveBelowThreshold = 0;
        if (st.consecutiveAboveThreshold < debounceSamples)
            ++st.consecutiveAboveThreshold;
        if (st.consecutiveAboveThreshold >= debounceSamples)
            st.detected = true;
        return st.detected;
    }

    // (1)(2) Esik alti: acilis sayacini sifirla. Bayrak zaten kapaliysa is yok.
    st.consecutiveAboveThreshold = 0;
    if (!st.detected) {
        st.consecutiveBelowThreshold = 0;
        return false;
    }

    // (3) RELEASE: bayrak yaniyorsa yalniz N ardisik esik-alti ornekten sonra dus.
    if (st.consecutiveBelowThreshold < releaseSamples)
        ++st.consecutiveBelowThreshold;
    if (st.consecutiveBelowThreshold >= releaseSamples) {
        st.consecutiveBelowThreshold = 0;
        st.detected = false;
    }
    return st.detected;
}

inline bool update(State& st, int32_t bmsCurrentCentiA, bool bmsDataValid,
                   int16_t motorRpm) {
    return updateImpl(st, bmsCurrentCentiA, bmsDataValid, motorRpm,
                      CHARGE_DETECT_CURRENT_CENTI_A,
                      CHARGE_DETECT_DEBOUNCE_SAMPLES, CHARGE_DETECT_MAX_RPM,
                      CHARGE_DETECT_RELEASE_SAMPLES);
}

}  // namespace ChargeDetect
