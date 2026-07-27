#include <unity.h>

#include <cstdint>

#include "ChargeState.h"

// Nextion `chg` alanı — şarj/deşarj/boşta karar mantığı (ChargeState.h).
// Vakalar saha ölçümüne (Y20, bkz. ChargeDetect.h ve SystemConfig.h
// CHARGE_DETECT_* yorumları) dayanır:
//   şarjda   +9.8 A  = +980 centi-A
//   boşta    -0.1 A  =  -10 centi-A   ← ölü bant olmadan "Desarj" yazardı
//   sürüşte  -5.6 A  = -560 centi-A
namespace {
constexpr int32_t DEADBAND = 100;      // HMI_CHG_DISCHARGE_DEADBAND_CENTI_A (1.0 A)
constexpr int32_t CUR_CHARGING = 980;  // saha: şarjda +9.8 A
constexpr int32_t CUR_DRIVING = -560;  // saha: sürüşte -5.6 A
constexpr int32_t CUR_IDLE = -10;      // saha: boşta -0.1 A
}  // namespace

// --- 1. dal: bayat/geçersiz veri HER ŞEYİ ezer (veri uydurma yok) ---

// bmsDataValid=false iken chargerActive=true olsa BİLE NO_DATA döner: bayat
// veriden durum TÜRETİLMEZ (Documents/CELL_VOLTAGE_INVESTIGATION.md
// "Mutlak Kural"). Öncelik sırasının 1. maddesinin kilidi.
void test_chg_invalid_bms_overrides_charger_active(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_NO_DATA,
        hmi_chargeState(/*bmsDataValid=*/false, /*bmsTimeoutActive=*/false,
                        /*chargerActive=*/true, CUR_CHARGING, DEADBAND));
}

// Timeout aktifken (E000/E001 bayatladı) veri "geçerli" işaretli olsa bile
// NO_DATA — freshness kaybı da veri yokluğudur.
void test_chg_timeout_active_returns_no_data(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_NO_DATA,
        hmi_chargeState(/*bmsDataValid=*/true, /*bmsTimeoutActive=*/true,
                        /*chargerActive=*/false, CUR_DRIVING, DEADBAND));
}

// --- 2. dal: şarj ---

// chargerActive=true + saha şarj akımı (+9.8 A) → CHARGING.
void test_chg_charger_active_returns_charging(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_CHARGING,
        hmi_chargeState(/*bmsDataValid=*/true, /*bmsTimeoutActive=*/false,
                        /*chargerActive=*/true, CUR_CHARGING, DEADBAND));
}

// ÖNCELİK KİLİDİ: chargerActive akımdan ÖNCE gelir — bayrak açıkken akım
// negatif olsa bile CHARGING kazanır (deşarj dalına DÜŞMEZ). Şarj kararının
// sahibi ChargeDetect/charger-frame katmanıdır, ham akım işareti değil.
void test_chg_charger_active_wins_over_negative_current(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_CHARGING,
        hmi_chargeState(/*bmsDataValid=*/true, /*bmsTimeoutActive=*/false,
                        /*chargerActive=*/true, CUR_DRIVING, DEADBAND));
}

// --- 3. dal: deşarj + ölü bant ---

// Saha sürüş akımı (-5.6 A) ölü bandın çok altında → DISCHARGING.
void test_chg_driving_current_returns_discharging(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_DISCHARGING,
        hmi_chargeState(/*bmsDataValid=*/true, /*bmsTimeoutActive=*/false,
                        /*chargerActive=*/false, CUR_DRIVING, DEADBAND));
}

// ÖLÜ BANT REGRESYONU: boşta ölçülen -0.1 A ölü bandın İÇİNDE → IDLE.
// Bu test olmadan araç dururken ekran kalıcı olarak "Desarj" yazar.
void test_chg_idle_current_within_deadband_returns_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_IDLE,
        hmi_chargeState(/*bmsDataValid=*/true, /*bmsTimeoutActive=*/false,
                        /*chargerActive=*/false, CUR_IDLE, DEADBAND));
}

// SINIR SEMANTİĞİ: akım tam -ölü bant değerinde → DISCHARGING (sınır DAHİL,
// "<=" semantiği kilitlenir; ">" ile karıştırılırsa bu test kırılır).
void test_chg_current_exactly_at_deadband_is_discharging(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_DISCHARGING,
        hmi_chargeState(/*bmsDataValid=*/true, /*bmsTimeoutActive=*/false,
                        /*chargerActive=*/false, -DEADBAND, DEADBAND));
}

// Sınırın bir birim içi (-99) hâlâ IDLE — ölü bandın üst ucu.
void test_chg_current_just_inside_deadband_is_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_IDLE,
        hmi_chargeState(/*bmsDataValid=*/true, /*bmsTimeoutActive=*/false,
                        /*chargerActive=*/false, -DEADBAND + 1, DEADBAND));
}

// --- 4. dal: boşta ---

// Akım tam sıfır ve şarj yok → IDLE.
void test_chg_zero_current_returns_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_IDLE,
        hmi_chargeState(/*bmsDataValid=*/true, /*bmsTimeoutActive=*/false,
                        /*chargerActive=*/false, 0, DEADBAND));
}

// REJENERATİF FRENLEME GÜVENLİĞİ: chargerActive=false iken POZİTİF akım
// (rejen; MOTOR_DRIVER_PRESENT=0 olduğu için bugün yok) "şarj" GÖSTERMEZ —
// şarj kararı yalnız chargerActive'ten gelir. Deşarj dalı da yalnız negatif
// akıma baktığından "Desarj" da yazmaz; sonuç IDLE (doğru ve güvenli).
void test_chg_positive_current_without_charger_flag_is_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(
        HMI_CHG_IDLE,
        hmi_chargeState(/*bmsDataValid=*/true, /*bmsTimeoutActive=*/false,
                        /*chargerActive=*/false, CUR_CHARGING, DEADBAND));
}

// Sözleşme kilidi: sayısal değerler Nextion tm0 timer'ındaki eşlemeyle
// birebir aynı olmalı (0=Bosta, 1=Sarj Oluyor, 2=Desarj, 3="--").
void test_chg_enum_values_match_nextion_contract(void) {
    TEST_ASSERT_EQUAL_UINT8(0, HMI_CHG_IDLE);
    TEST_ASSERT_EQUAL_UINT8(1, HMI_CHG_CHARGING);
    TEST_ASSERT_EQUAL_UINT8(2, HMI_CHG_DISCHARGING);
    TEST_ASSERT_EQUAL_UINT8(3, HMI_CHG_NO_DATA);
}
