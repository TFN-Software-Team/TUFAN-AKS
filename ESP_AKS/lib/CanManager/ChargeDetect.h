#pragma once
#include <cstdint>

#include "SystemConfig.h"  // CHARGE_DETECT_CURRENT_CENTI_A, _DEBOUNCE_SAMPLES, _MAX_RPM

// ---------------------------------------------------------------------------
// Akim tabanli sarj tespiti — SAF karar mantigi (Y20 gozlemi).
// ---------------------------------------------------------------------------
// NEDEN VAR: `TEL_chargerActive`in BIRINCIL kaynagi, charger komut frame'inin
// (0x1806E5F4) tazeligidir (CanManager::CAN_chargerValid). Ancak bu akis
// OPSIYONELDIR — charger CAN'e hic konusmuyorsa veya baska bir ID kullaniyorsa
// sarj HIC fark edilmez ve sartname 8.2.a.iii (sarjda S1 KAPALI + S2 ACIK)
// sessizce uygulanmaz. Bu baslik, o durumda devreye giren BAGIMSIZ bir
// gostergeyi saglar.
//
// GOSTERGE (ekip PCAN olcumu, Y20 — bkz. BENI_OKU.md 5.3 "DOGRULANDI"):
//   sarjda   +9.8 A  (POZITIF — batarya akim ALIYOR)
//   bosta    -0.1 A
//   suruste  -5.6 A  (NEGATIF)
// Akim alaninin kaynagi (0xE000 byte[0:1]) DOGRULANMIS durumdadir.
//
// ONCELIK: CAN kaynagi (CAN_chargerValid) BIRINCILDIR. Bu tespit yalnizca
// EK bir gostergedir ve OR'lanir — yani charger CAN'i gorunuyorsa zaten
// sarj sayilir, gorunmuyorsa akim isareti devreye girer.
//
// UC KORUMA KATMANI (yanlis pozitif = sarjda olmadigi halde S1'i kapatmak):
//   1. ESIK    : yalniz POZITIF ve CHARGE_DETECT_CURRENT_CENTI_A ustu akim.
//                Desarj (negatif) ASLA sarj sayilmaz.
//   2. DEBOUNCE: CHARGE_DETECT_DEBOUNCE_SAMPLES ardisik ornek gerekir; tek
//                orneklik gurultu/tepe bayragi actirmaz. Bayrak, esik altina
//                inen ILK ornekte hemen duser (asimetrik: acilis yavas,
//                kapanis hizli — guvenli taraf).
//   3. HAREKETSIZLIK: motor surucusu geldiginde REJENERATIF FRENLEME de
//                bataryaya POZITIF akim basar. Rejeni "sarj" sanmamak icin
//                arac fiilen DURUYOR olmalidir (|rpm| < CHARGE_DETECT_MAX_RPM).
//                Bugun MOTOR_DRIVER_PRESENT=0 oldugundan rejen yok, ama bu
//                kapi simdiden konuldu ki motor entegrasyonunda unutulmasin.
// Ayrica BMS verisi taze degilse (bmsDataValid=false) akim son gorulen bayat
// degeri tutar — o degerden karar URETILMEZ.
//
// Durum (ardisik ornek sayaci) cagirana aittir; ayni idiom:
// lib/VcuLogic/HeadlightSwitch.h, lib/DisplayHMI/ResyncPolicy.h.
namespace ChargeDetect {

struct State {
    uint8_t consecutiveAboveThreshold;  // ust uste esigi asan ornek sayisi
    bool detected;                      // debounce sonrasi karar (cikti)
};

inline State makeState() { return State{0, false}; }

// Parametreli (test edilebilir) cekirdek. Uretim kodu asagidaki update()
// sarmalayicisini kullanir (SysStateDerive::deriveFromCurrentImpl ile ayni desen).
inline bool updateImpl(State& st, int32_t bmsCurrentCentiA, bool bmsDataValid,
                       int16_t motorRpm, int32_t thresholdCentiA,
                       uint8_t debounceSamples, int16_t maxRpm) {
    const bool stationary = (motorRpm < maxRpm) && (motorRpm > -maxRpm);
    const bool aboveThreshold =
        bmsDataValid && stationary && (bmsCurrentCentiA > thresholdCentiA);

    if (!aboveThreshold) {
        // Esik altina inen ILK ornekte hemen dus (guvenli taraf).
        st.consecutiveAboveThreshold = 0;
        st.detected = false;
        return st.detected;
    }

    if (st.consecutiveAboveThreshold < debounceSamples)
        ++st.consecutiveAboveThreshold;

    st.detected = (st.consecutiveAboveThreshold >= debounceSamples);
    return st.detected;
}

inline bool update(State& st, int32_t bmsCurrentCentiA, bool bmsDataValid,
                   int16_t motorRpm) {
    return updateImpl(st, bmsCurrentCentiA, bmsDataValid, motorRpm,
                      CHARGE_DETECT_CURRENT_CENTI_A,
                      CHARGE_DETECT_DEBOUNCE_SAMPLES, CHARGE_DETECT_MAX_RPM);
}

}  // namespace ChargeDetect
