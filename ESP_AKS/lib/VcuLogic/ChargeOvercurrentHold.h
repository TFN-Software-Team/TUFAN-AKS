#pragma once
#include <cstdint>

#include "SystemConfig.h"  // BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A, BMS_CHARGE_OVERCURRENT_HOLD_MS

// ---------------------------------------------------------------------------
// SARJ YONU ASIRI AKIM — KESINTISIZ SURE KAPISI (SAF karar mantigi).
// ---------------------------------------------------------------------------
// SORUN (saha, sürüş denemesi): gaz pedali koklenip ANIDEN birakildiginda motor
// bir an jeneratore doner (rejen / geri-EMK) ve batarya akimi POZITIF yonde
// +20 / +30 / +40 A'e ANLIK olarak firlar. isCurrentCritical'in sarj dali
// (>= BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A = 13.0 A) TEK ORNEKTE saglanip
// hasCriticalCondition'i true yapiyor, run() de aracin READY/DRIVE'dan aninda
// FAULT'a gecmesine — kontaktorlerin acilmasina — sebep oluyordu. Yani gecici
// bir rejen tepesi "asiri sarj akimi" (arac sarjda) sanildi.
//
// COZUM: sarj yonundeki asiri akim artik ZAMAN ile nitelenir. Akimin
// BMS_CHARGE_OVERCURRENT_HOLD_MS (10 sn) boyunca KESINTISIZ esik ustunde
// kalmasi gerekir; seri bir kez kirilirsa (akim esigin altina iner, sifira ya
// da desarj yonune gecer) sayac SIFIRDAN baslar. Gercek bir sarj/asiri sarj
// akimi doga geregi sureklidir ve 10 sn'yi rahatlikla doldurur; rejen tepesi
// (~1 sn'nin altinda) asla dolduramaz.
//
// KAPSAM — KASITLI OLARAK DAR:
//   * YALNIZ POZITIF (sarj) yonu gecikir. DESARJ tarafi
//     (<= -BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_A) ANINDA FAULT uretmeye
//     devam eder: asiri desarj gercek bir kisa-devre/asiri yuk gostergesidir ve
//     10 sn beklenemez.
//   * Diger kritik kosullar (pack voltaji, sicaklik, hucre voltaji, freshness)
//     DEGISMEDI — hepsi eskisi gibi anindadir.
//   * Kapi yalnizca FAULT'a GIRISI geciktirir. FAULT'tan CIKIS (reset
//     interlock) ve READY girisi ANLIK sarj-akimi kontrolunu kullanmaya devam
//     eder (bkz. VcuLogic.h hasCriticalCondition varsayilan argumani): gecikme
//     bir arizadan kacmanin yolu olmamalidir.
//
// BAYAT VERI KURALI (EK B GUVEN, CLAUDE.md Kural 4): bmsDataValid=false iken
// akimdan karar URETILMEZ — seri kirilir (sayac sifirlanir). CanManager
// freshness kaybinda TEL_bmsCurrentCentiA'yi SIFIRLAMAZ (son deger donar);
// donmus bir degerin 10 sn'lik sayaci doldurmasina izin verilemez. Bayat BMS
// verisinin kendisi zaten READY/DRIVE'da ayri ve ANINDA bir kritik kosuldur
// (TEL_bmsTimeoutActive → hasCriticalCondition).
//
// Durum (zaman damgasi) cagirana aittir; ayni idiom: lib/CanManager/
// ChargeDetect.h, lib/VcuLogic/HeadlightSwitch.h. Zaman tabani cagiranin
// monoton ms sayacidir (VcuLogic::run icinde s_uptimeMs); isaretsiz cikarma
// idiomu uint32_t sarmasinda da dogru calisir.
namespace ChargeOvercurrentHold {

struct State {
    bool above;        // son ornek esigin ustunde miydi (seri suruyor mu)
    uint32_t sinceMs;  // kesintisiz serinin BASLANGIC damgasi
    bool confirmed;    // sure doldu — artik gercek asiri sarj akimi sayilir
};

inline State makeState() { return State{false, 0, false}; }

// Parametreli (test edilebilir) cekirdek. Uretim kodu asagidaki update()
// sarmalayicisini kullanir (ChargeDetect::updateImpl ile ayni desen).
inline bool updateImpl(State& st, int32_t bmsCurrentCentiA, bool bmsDataValid,
                       uint32_t nowMs, int32_t thresholdCentiA,
                       uint32_t holdMs) {
    // Esik semantigi >= : isCurrentCritical'in sarj daliyla BIREBIR ayni olmali
    // (iki yerde iki farkli "asiri sarj akimi" tanimi olmasin).
    const bool above = bmsDataValid && (bmsCurrentCentiA >= thresholdCentiA);

    if (!above) {
        st.above = false;
        st.sinceMs = 0;
        st.confirmed = false;
        return false;
    }

    if (!st.above) {
        // Serinin ILK ornegi — sayaci buradan baslat.
        st.above = true;
        st.sinceMs = nowMs;
    }

    st.confirmed = (uint32_t)(nowMs - st.sinceMs) >= holdMs;
    return st.confirmed;
}

inline bool update(State& st, int32_t bmsCurrentCentiA, bool bmsDataValid,
                   uint32_t nowMs) {
    return updateImpl(st, bmsCurrentCentiA, bmsDataValid, nowMs,
                      BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A,
                      BMS_CHARGE_OVERCURRENT_HOLD_MS);
}

}  // namespace ChargeOvercurrentHold
