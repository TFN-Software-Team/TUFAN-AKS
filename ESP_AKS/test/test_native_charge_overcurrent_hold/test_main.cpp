// Sarj yonu asiri akim — KESINTISIZ SURE KAPISI
// (lib/VcuLogic/ChargeOvercurrentHold.h saf mantigi + hasCriticalCondition
// entegrasyonu).
//
// SAHA SORUNU: gaz pedali koklenip ANIDEN birakilinca rejen/geri-EMK batarya
// akimini ANLIK olarak +20 / +30 / +40 A'e firlatiyor. Eski davranista bu tek
// ornek BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A'yi (1300 = 13.0 A) asiyor,
// hasCriticalCondition true donuyor ve arac READY/DRIVE'dan aninda FAULT'a
// gecip kontaktorleri aciyordu.
// BEKLENEN: eşik ustu POZITIF akim BMS_CHARGE_OVERCURRENT_HOLD_MS (10 sn)
// KESINTISIZ surmedikce kritik SAYILMAZ; desarj tarafi ve diger tum kritik
// kosullar ANINDA kalir.
#include <unity.h>

#include "ChargeOvercurrentHold.h"
#include "VcuLogic.h"
#include "test_helpers.h"

using VcuLogic::hasCriticalCondition;
using VcuLogic::VcuState;

namespace {

constexpr uint32_t TICK_MS = 20;  // VCU tik periyodu (main.cpp, 50 Hz)

// Ayni akimi n tik boyunca uygular; nowMs cagiranin monoton sayacini taklit
// eder (uretimde s_uptimeMs). Son sonucu dondurur.
bool feed(ChargeOvercurrentHold::State& st, int32_t currentCentiA, bool valid,
          uint32_t& nowMs, int ticks) {
    bool out = false;
    for (int i = 0; i < ticks; ++i) {
        nowMs += TICK_MS;
        out = ChargeOvercurrentHold::update(st, currentCentiA, valid, nowMs);
    }
    return out;
}

// 10 sn = 500 tik (10000 / 20). SERININ ILK ORNEGI t0 damgasidir ve onay
// "t0'dan bu yana >= 10 sn" oldugunda gelir; yani ilk ornek + 500 tik = 501
// ornek gerekir. Bu bilincli: sure ORNEK SAYISI degil GECEN ZAMAN olcer.
constexpr int HOLD_TICKS = (int)(BMS_CHARGE_OVERCURRENT_HOLD_MS / TICK_MS);
constexpr int CONFIRM_TICKS = HOLD_TICKS + 1;

}  // namespace

// --- SAHA REGRESYONU: rejen tepesi ----------------------------------------

// Gaz kesmedeki +20 A'lik anlik tepe (yaklasik 0.5 sn = 25 tik) ONAYLANMAMALI.
void test_regen_spike_20A_does_not_confirm(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_FALSE(feed(st, 2000, true, now, 25));
    // Tepe gecti — akim surus yonune (-5.6 A) dondu.
    TEST_ASSERT_FALSE(feed(st, -560, true, now, 5));
}

// +40 A'lik daha sert tepe de sure dolmadan onaylanmaz.
void test_regen_spike_40A_does_not_confirm(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_FALSE(feed(st, 4000, true, now, 50));  // ~1 sn
}

// Arka arkaya gelen tepeler ARASI kesildigi surece sayac her seferinde
// SIFIRDAN baslar — birikmez (surus boyunca defalarca gaz kesmek FAULT uretmez).
void test_repeated_spikes_never_accumulate(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    for (int i = 0; i < 20; ++i) {
        TEST_ASSERT_FALSE(feed(st, 3000, true, now, 100));  // 2 sn tepe
        TEST_ASSERT_FALSE(feed(st, -560, true, now, 1));    // tek ornek kesinti
    }
}

// --- Sure kapisinin siniri -------------------------------------------------

// Sure dolmadan bir tik once (9.98 sn) HENUZ onaylanmaz.
void test_not_confirmed_one_tick_before_hold(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_FALSE(feed(st, 2000, true, now, CONFIRM_TICKS - 1));
}

// Kesintisiz 10 sn dolunca ONAYLANIR (gercek asiri sarj akimi hala FAULT uretir).
void test_confirmed_exactly_at_hold(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed(st, 2000, true, now, CONFIRM_TICKS));
}

// Onaydan sonra akim esik ustunde kaldikca onay SURER.
void test_stays_confirmed_while_current_remains_high(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed(st, 2000, true, now, CONFIRM_TICKS));
    TEST_ASSERT_TRUE(feed(st, 1400, true, now, 10));
}

// TEK kesinti ornegi seriyi kirar: sayac SIFIRDAN baslar (yeniden 10 sn gerekir).
void test_single_interrupting_sample_restarts_counter(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_FALSE(feed(st, 2000, true, now, CONFIRM_TICKS - 1));
    TEST_ASSERT_FALSE(feed(st, 0, true, now, 1));  // seri kirildi
    TEST_ASSERT_FALSE(feed(st, 2000, true, now, CONFIRM_TICKS - 1));
    TEST_ASSERT_TRUE(feed(st, 2000, true, now, 1));
}

// Onay verilmisken akim esik altina inerse onay DUSER (release gecikmesi YOK —
// kritik kosul artik gecerli degildir).
void test_confirmation_drops_immediately_when_current_falls(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_TRUE(feed(st, 2000, true, now, CONFIRM_TICKS));
    TEST_ASSERT_FALSE(feed(st, 1290, true, now, 1));  // 12.9 A — esik alti
}

// Esik semantigi >= : eşigin KENDISI seriyi baslatir, bir centi-A altisi baslatmaz.
void test_threshold_semantics_match_is_charge_current_critical(void) {
    uint32_t now = 0;
    ChargeOvercurrentHold::State at = ChargeOvercurrentHold::makeState();
    TEST_ASSERT_TRUE(feed(at, BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A, true, now,
                          CONFIRM_TICKS));

    now = 0;
    ChargeOvercurrentHold::State below = ChargeOvercurrentHold::makeState();
    TEST_ASSERT_FALSE(feed(below, BMS_CRITICAL_MAX_CHARGE_CURRENT_CENTI_A - 1,
                           true, now, CONFIRM_TICKS * 2));
}

// Sahada gozlenen NOMINAL sarj akimi (+9.9 A) esigin ALTINDA — ne kadar surerse
// sursun kritik olmaz (kapi normal sarji FAULT'a cevirmez).
void test_nominal_charging_current_never_confirms(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_FALSE(feed(st, 990, true, now, CONFIRM_TICKS * 3));
}

// --- Bayat veri kurali (EK B GUVEN) ---------------------------------------

// bmsDataValid=false iken akimdan karar URETILMEZ: seri baslamaz.
void test_stale_data_never_starts_series(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_FALSE(feed(st, 4000, false, now, CONFIRM_TICKS * 2));
}

// Seri suruyorken veri bayatlarsa seri KIRILIR (donmus deger sayaci dolduramaz).
void test_stale_data_breaks_running_series(void) {
    ChargeOvercurrentHold::State st = ChargeOvercurrentHold::makeState();
    uint32_t now = 0;
    TEST_ASSERT_FALSE(feed(st, 2000, true, now, CONFIRM_TICKS - 1));
    TEST_ASSERT_FALSE(feed(st, 2000, false, now, 1));  // bayat — seri kirildi
    TEST_ASSERT_FALSE(feed(st, 2000, true, now, CONFIRM_TICKS - 1));
}

// --- hasCriticalCondition entegrasyonu ------------------------------------

// Kapi ACIK degilken (held=false) esik ustu POZITIF akim kritik SAYILMAZ.
void test_critical_condition_ignores_unheld_charge_overcurrent(void) {
    TelemetryData d = test_helpers::makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 4000;  // +40 A rejen tepesi
    TEST_ASSERT_FALSE(hasCriticalCondition(d, VcuState::DRIVE, false));
}

// Kapi onayladiginda (held=true) ayni akim KRITIKTIR — FAULT uretir.
void test_critical_condition_faults_on_held_charge_overcurrent(void) {
    TelemetryData d = test_helpers::makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 4000;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::DRIVE, true));
}

// DESARJ tarafi kapidan ETKILENMEZ: held=false olsa da ANINDA kritiktir
// (asiri yuk / kisa devre 10 sn bekleyemez).
void test_discharge_overcurrent_stays_immediate(void) {
    TelemetryData d = test_helpers::makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = -BMS_CRITICAL_MAX_DISCHARGE_CURRENT_CENTI_A;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::DRIVE, false));
}

// Diger kritik kosullar (ornek: sicaklik) kapidan ETKILENMEZ.
void test_other_critical_conditions_stay_immediate(void) {
    TelemetryData d = test_helpers::makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = BMS_CRITICAL_MAX_TEMP_C;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::DRIVE, false));
}

// VARSAYILAN argüman ESKI (anlik) davranistir — reset interlock ve READY girisi
// bunu kullanir: zaman kapisi FAULT'tan CIKMANIN yolu OLAMAZ.
void test_default_argument_keeps_instant_behaviour(void) {
    TelemetryData d = test_helpers::makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 4000;
    TEST_ASSERT_TRUE(hasCriticalCondition(d, VcuState::DRIVE));
    TEST_ASSERT_FALSE(VcuLogic::isResetInterlockSatisfied(d, VcuState::FAULT));
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_regen_spike_20A_does_not_confirm);
    RUN_TEST(test_regen_spike_40A_does_not_confirm);
    RUN_TEST(test_repeated_spikes_never_accumulate);
    RUN_TEST(test_not_confirmed_one_tick_before_hold);
    RUN_TEST(test_confirmed_exactly_at_hold);
    RUN_TEST(test_stays_confirmed_while_current_remains_high);
    RUN_TEST(test_single_interrupting_sample_restarts_counter);
    RUN_TEST(test_confirmation_drops_immediately_when_current_falls);
    RUN_TEST(test_threshold_semantics_match_is_charge_current_critical);
    RUN_TEST(test_nominal_charging_current_never_confirms);
    RUN_TEST(test_stale_data_never_starts_series);
    RUN_TEST(test_stale_data_breaks_running_series);
    RUN_TEST(test_critical_condition_ignores_unheld_charge_overcurrent);
    RUN_TEST(test_critical_condition_faults_on_held_charge_overcurrent);
    RUN_TEST(test_discharge_overcurrent_stays_immediate);
    RUN_TEST(test_other_critical_conditions_stay_immediate);
    RUN_TEST(test_default_argument_keeps_instant_behaviour);
    return UNITY_END();
}
