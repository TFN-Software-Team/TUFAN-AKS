// Akim tabanli sarj tespiti (Y20) — lib/CanManager/ChargeDetect.h saf mantigi.
//
// Ekip PCAN olcumu (Y20, BENI_OKU.md 5.3 "DOGRULANDI"):
//   sarjda   +9.8 A = +980 centi-A   (POZITIF — batarya akim ALIYOR)
//   bosta    -0.1 A =  -10 centi-A
//   suruste  -5.6 A = -560 centi-A   (NEGATIF)
// Uretim esigi CHARGE_DETECT_CURRENT_CENTI_A = +200 centi-A (2.0 A).
#include <unity.h>

#include "ChargeDetect.h"

namespace {

// Debounce'i doldurmak icin ayni girdiyi N kez uygula, son sonucu dondur.
bool feed(ChargeDetect::State& st, int32_t currentCentiA, bool bmsValid,
          int16_t rpm, int n) {
    bool out = false;
    for (int i = 0; i < n; ++i)
        out = ChargeDetect::update(st, currentCentiA, bmsValid, rpm);
    return out;
}

}  // namespace

// --- Temel esik davranisi -------------------------------------------------

// Y20 sarj akimi (+980) debounce dolunca sarj sayilir.
void test_charge_detected_at_observed_charging_current(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_TRUE(feed(st, 980, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
}

// Y20 bosta akimi (-10) sarj SAYILMAZ.
void test_no_charge_at_observed_idle_current(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_FALSE(feed(st, -10, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
}

// Y20 surus akimi (-560) sarj SAYILMAZ — negatif asla sarj degildir.
void test_no_charge_at_observed_drive_current(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_FALSE(feed(st, -560, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
}

// Tam esikte sarj sayilmaz (kural: esigin USTUNDE olmali).
void test_no_charge_exactly_at_threshold(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_FALSE(feed(st, CHARGE_DETECT_CURRENT_CENTI_A, true, 0,
                           CHARGE_DETECT_DEBOUNCE_SAMPLES));
}

// Esigin bir birim ustunde sarj sayilir.
void test_charge_just_above_threshold(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_TRUE(feed(st, CHARGE_DETECT_CURRENT_CENTI_A + 1, true, 0,
                          CHARGE_DETECT_DEBOUNCE_SAMPLES));
}

// --- Debounce -------------------------------------------------------------

// Tek ornek YETMEZ: debounce dolmadan bayrak acilmaz (gurultu korumasi).
void test_single_sample_does_not_trip_debounce(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_FALSE(ChargeDetect::update(st, 980, true, 0));
}

// Debounce'un TAM sinirinda acilir: N-1 ornekte kapali, N'inci ornekte acik.
void test_debounce_opens_exactly_at_sample_count(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_FALSE(feed(st, 980, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES - 1));
    TEST_ASSERT_TRUE(ChargeDetect::update(st, 980, true, 0));
}

// Debounce sayaci ARDISIK olmali: arada esik alti bir ornek sayaci sifirlar.
void test_debounce_resets_on_interrupting_sample(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    feed(st, 980, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES - 1);
    TEST_ASSERT_FALSE(ChargeDetect::update(st, -10, true, 0));  // kesinti
    // Sayac sifirlandigi icin tek bir yuksek ornek yine yetmemeli.
    TEST_ASSERT_FALSE(ChargeDetect::update(st, 980, true, 0));
}

// --- Release (kapanis) debounce'u -----------------------------------------
// 2026-07-29: 0x1806E5F4 tazeligi TEL_chargerActive'den cikarildi (SORUN 1),
// akim artik TEK gosterge. Bu yuzden kapanis de debounce'lu: sarj sonu
// CV/taper fazinda akim dogal olarak esigin altina inip cikar; tek ornekte
// dusmek "SARJ OLUYOR" gostergesini ve S1 kararlarini cirpindirirdi.

// Esik altina inen TEK ornek bayragi DUSURMEZ (eski davranisin tersi).
void test_single_below_sample_does_not_drop_detection(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_TRUE(feed(st, 980, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
    TEST_ASSERT_TRUE(ChargeDetect::update(st, -10, true, 0));
}

// Release TAM sinirinda duser: N-1 ornekte hala acik, N'inci ornekte kapali.
void test_release_drops_exactly_at_sample_count(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_TRUE(feed(st, 980, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
    TEST_ASSERT_TRUE(feed(st, -10, true, 0, CHARGE_DETECT_RELEASE_SAMPLES - 1));
    TEST_ASSERT_FALSE(ChargeDetect::update(st, -10, true, 0));
}

// Release sayaci ARDISIK olmali: taper dalgalanmasinda (arada esik ustu bir
// ornek) sayac sifirlanir ve bayrak yanik kalir.
void test_release_counter_resets_on_current_recovery(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_TRUE(feed(st, 980, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
    TEST_ASSERT_TRUE(feed(st, -10, true, 0, CHARGE_DETECT_RELEASE_SAMPLES - 1));
    TEST_ASSERT_TRUE(ChargeDetect::update(st, 980, true, 0));  // taper toparladi
    // Sayac sifirlandigi icin tek bir esik-alti ornek yine dusurmemeli.
    TEST_ASSERT_TRUE(ChargeDetect::update(st, -10, true, 0));
}

// --- Guvenlik kapilari ----------------------------------------------------

// BMS verisi taze DEGILSE akim bayattir — karar URETILMEZ.
void test_no_charge_when_bms_data_stale(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_FALSE(feed(st, 980, false, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
}

// REJEN KORUMASI: arac hareket halindeyken pozitif akim sarj SAYILMAZ
// (motor surucusu geldiginde rejeneratif frenleme de pozitif akim basar).
void test_no_charge_while_vehicle_moving_forward(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_FALSE(feed(st, 980, true, CHARGE_DETECT_MAX_RPM,
                           CHARGE_DETECT_DEBOUNCE_SAMPLES));
}

// Rejen korumasi geri yonde de gecerli (negatif rpm).
void test_no_charge_while_vehicle_moving_backward(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_FALSE(feed(st, 980, true, -CHARGE_DETECT_MAX_RPM,
                           CHARGE_DETECT_DEBOUNCE_SAMPLES));
}

// Hareketsizlik esiginin hemen ALTINDA (rolanti titresimi) sarj tespiti calisir.
void test_charge_detected_just_below_motion_threshold(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_TRUE(feed(st, 980, true, CHARGE_DETECT_MAX_RPM - 1,
                          CHARGE_DETECT_DEBOUNCE_SAMPLES));
}

// Sarj sirasinda BMS verisi kesilirse bayrak release debounce'u kadar sonra
// DUSER (bayat akimla SURESIZ surdurulmez). Bayat veri "esik alti" gibi islenir.
void test_detection_drops_when_bms_goes_stale_mid_charge(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_TRUE(feed(st, 980, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
    TEST_ASSERT_TRUE(feed(st, 980, false, 0, CHARGE_DETECT_RELEASE_SAMPLES - 1));
    TEST_ASSERT_FALSE(ChargeDetect::update(st, 980, false, 0));
}

// HAREKET KAPISI release debounce'undan MUAFTIR: arac hareket ettigi ANDA
// bayrak duser. Rejeneratif frenleme akimi hicbir kosulda ~2 sn boyunca
// "sarj" sanilmaz — guvenlik yonu asimetrik kalir.
void test_motion_drops_detection_immediately_bypassing_release(void) {
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_TRUE(feed(st, 980, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
    TEST_ASSERT_FALSE(ChargeDetect::update(st, 980, true, CHARGE_DETECT_MAX_RPM));
}

// --- SORUN 1 regresyonu (2026-07-29) --------------------------------------
// Iki canli CAN kaydindan alinan GERCEK akim degerleriyle uctan uca ayrim.
// 0x1806E5F4 her iki kayitta da ~100 ms'de bir akiyordu; tespitin TEK dayanagi
// artik asagidaki akim farkidir.
void test_logged_idle_current_never_detects_charge(void) {
    // esp32-session.log (SARJDA DEGIL): 0xE000 byte[0:1] = FF FF / FF FE
    ChargeDetect::State st = ChargeDetect::makeState();
    for (int i = 0; i < 50; ++i) {
        TEST_ASSERT_FALSE(ChargeDetect::update(st, (i % 2) ? -10 : -20, true, 0));
    }
}

void test_logged_charging_current_detects_charge(void) {
    // batarya_tam_kayit(1).log (SARJDA): 0xE000 byte[0:1] = 00 5B … 00 64
    ChargeDetect::State st = ChargeDetect::makeState();
    TEST_ASSERT_TRUE(feed(st, 910, true, 0, CHARGE_DETECT_DEBOUNCE_SAMPLES));
    TEST_ASSERT_TRUE(ChargeDetect::update(st, 1000, true, 0));
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_charge_detected_at_observed_charging_current);
    RUN_TEST(test_no_charge_at_observed_idle_current);
    RUN_TEST(test_no_charge_at_observed_drive_current);
    RUN_TEST(test_no_charge_exactly_at_threshold);
    RUN_TEST(test_charge_just_above_threshold);
    RUN_TEST(test_single_sample_does_not_trip_debounce);
    RUN_TEST(test_debounce_opens_exactly_at_sample_count);
    RUN_TEST(test_debounce_resets_on_interrupting_sample);
    RUN_TEST(test_single_below_sample_does_not_drop_detection);
    RUN_TEST(test_release_drops_exactly_at_sample_count);
    RUN_TEST(test_release_counter_resets_on_current_recovery);
    RUN_TEST(test_no_charge_when_bms_data_stale);
    RUN_TEST(test_no_charge_while_vehicle_moving_forward);
    RUN_TEST(test_no_charge_while_vehicle_moving_backward);
    RUN_TEST(test_charge_detected_just_below_motion_threshold);
    RUN_TEST(test_detection_drops_when_bms_goes_stale_mid_charge);
    RUN_TEST(test_motion_drops_detection_immediately_bypassing_release);
    RUN_TEST(test_logged_idle_current_never_detects_charge);
    RUN_TEST(test_logged_charging_current_detects_charge);
    return UNITY_END();
}
