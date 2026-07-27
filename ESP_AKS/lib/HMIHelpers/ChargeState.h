#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Nextion `chg` alanı — şarj / deşarj / boşta gösterimi (SAF karar mantığı).
// ---------------------------------------------------------------------------
// Ekran tarafında `chg` gizli bir number bileşenidir; `tm0` timer'ı değerini
// `chgtxt` metnine çevirir (0="Bosta", 1="Sarj Oluyor", 2="Desarj", 3="--").
// Bu başlık o değeri üreten TEK karar noktasıdır — donanımsız, IDF'siz, native
// test edilir (test/test_native_hmi_helpers/test_charge_state.cpp).
//
// NEDEN BURADA VE NEDEN YENİDEN UYDURULMUYOR:
//
//   ŞARJ kararının kaynağı `TEL_chargerActive`tır. O bayrak ZATEN iki bağımsız
//   göstergenin OR'udur (bkz. CanManager.cpp::getTelemetryData ve
//   lib/CanManager/ChargeDetect.h):
//     1. charger komut frame'inin (0x1806E5F4) TAZELİĞİ — birincil, opsiyonel akış,
//     2. ChargeDetect'in akım-işareti tespiti — eşik + debounce + hareketsizlik
//        korumalarıyla.
//   Bu mantık NE EKRANDA NE BURADA yeniden üretilir; hazır bayrak tüketilir.
//   Aksi halde iki ayrı "şarjda mı?" tanımı oluşur ve sessizce ayrışırlar.
//
//   DEŞARJ için ham akım işaretine bakılır, ancak ÖLÜ BANT ŞARTTIR: saha
//   ölçümünde (Y20) boşta akım -0.1 A'dir (-10 centi-A). Ölü bant olmadan araç
//   dururken ekran kalıcı olarak "Desarj" yazar. Eşik CONFIG'tir:
//   HMI_CHG_DISCHARGE_DEADBAND_CENTI_A (SystemConfig.h) — bir GÖSTERİM eşiği,
//   güvenlik eşiği DEĞİL; FAULT/kontaktör kararına GİRMEZ.
//
//   REJENERATİF FRENLEME bataryaya POZİTİF akım basar. MOTOR_DRIVER_PRESENT=0
//   olduğu için bugün mevcut değildir; geldiğinde de şarj kararı zaten
//   ChargeDetect'in HAREKETSİZLİK katmanından geçtiğinden rejen "şarj"
//   sayılmaz. Deşarj dalı yalnızca NEGATİF akıma baktığından rejen "deşarj" da
//   göstermez — en fazla IDLE görünür. Bu doğru ve güvenli davranıştır.
//
//   VERİ UYDURMA YOK: BMS verisi geçersiz veya bayatsa hiçbir durum
//   TÜRETİLMEZ, NO_DATA basılır (Documents/CELL_VOLTAGE_INVESTIGATION.md
//   "Mutlak Kural"). Bayat akımdan "Bosta"/"Desarj" üretmek, ekranda gerçek
//   olmayan bir bilgi göstermek demektir.
//
// Referans desen: lib/CanManager/ChargeDetect.h, lib/VcuLogic/HeadlightSwitch.h.

// `chg.val` sözleşmesi — Nextion tm0 timer'ındaki eşlemeyle BİREBİR aynı
// olmalıdır (bkz. Documents/NEXTION_EKRAN_YAPILACAKLAR.md).
enum : uint8_t {
    HMI_CHG_IDLE = 0,         // "Bosta"
    HMI_CHG_CHARGING = 1,     // "Sarj Oluyor"
    HMI_CHG_DISCHARGING = 2,  // "Desarj"
    HMI_CHG_NO_DATA = 3       // "--" — BMS verisi yok/bayat, durum BİLİNMİYOR
};

// Karar sırası (ilk eşleşen kazanır — sıra semantiğin kendisidir):
//   1. BMS verisi geçersiz VEYA timeout  → NO_DATA (bayat veri her şeyi ezer)
//   2. chargerActive                     → CHARGING
//   3. akım <= -ölü bant                 → DISCHARGING (sınır DAHİL)
//   4. aksi halde                        → IDLE
inline uint8_t hmi_chargeState(bool bmsDataValid, bool bmsTimeoutActive,
                               bool chargerActive, int32_t packCurrentCentiA,
                               int32_t dischargeDeadbandCentiA) {
    if (!bmsDataValid || bmsTimeoutActive)
        return HMI_CHG_NO_DATA;

    if (chargerActive)
        return HMI_CHG_CHARGING;

    if (packCurrentCentiA <= -dischargeDeadbandCentiA)
        return HMI_CHG_DISCHARGING;

    return HMI_CHG_IDLE;
}
