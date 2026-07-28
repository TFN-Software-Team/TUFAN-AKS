#include <unity.h>

#include "VcuLogic.h"
#include "fake_freertos.h"
#include "mock_relay_actuator.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;
using VcuLogic::VcuEvent;
using VcuLogic::VcuState;

// ===========================================================================
// BOLUM C — KONTROLLU DURDURMA (ekran "DUR" butonu, HMI_CMD_STOP).
//
// Bu komuttan once READY/DRIVE'dan cikmanin TEK yolu E-STOP veya FAULT'tu;
// normal bir durus icin E-STOP'a basmak asiri tepkiydi (E-STOP kaydi duser,
// RESET interlock'u gerekir). STOP, E-STOP'un YERINI TUTMAZ:
//   E-STOP : acil, HER durumda calisir, kuyrugu bypass eder, EMERGENCY_STOP'a
//            gecer.
//   STOP   : normal, YALNIZ READY/DRIVE'da anlamli, IDLE'a doner, ariza kaydi
//            BIRAKMAZ.
// ===========================================================================

// ---------------------------------------------------------------------------
// GÜVENLİ KAPANIŞ SIRASI (P3): STOP da E-STOP/FAULT ile aynı sırayı izler —
// (1) sıfır tork → (2) VCU_CONTACTOR_OPEN_DELAY_MS bekle → (3) kontaktör aç.
// Bu yüzden STOP artık TEK bir run() tick'inde tamamlanmaz: ilk tick sıfır
// torku ister, kontaktör açma + IDLE dönüşü BİR SONRAKİ tick'te olur.
// ---------------------------------------------------------------------------
namespace {

// Torque sink spy'ı — sıfır-tork isteğinin sayısını, değerini ve çağrı sıra
// numarasını (mock allOff ile PAYLAŞILAN g_fake_call_seq üzerinden) kaydeder.
unsigned s_stopTorqueCount = 0;
unsigned s_stopTorqueFirstSeq = 0;
uint16_t s_stopLastTorque = 0xFFFF;

void stopTorqueSpy(uint16_t torque) {
    ++s_stopTorqueCount;
    s_stopLastTorque = torque;
    if (s_stopTorqueFirstSeq == 0)
        s_stopTorqueFirstSeq = ++g_fake_call_seq;
}

void primeIdle() {
    VcuLogic::resetForTest();
    fake_freertos_reset();
    fake_relay_reset();
    VcuLogic::init(g_mockRelay);
    VcuLogic::setTelemetryData(makeTelemetryDataValid());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

// Spy + sıra sayaçlarını temiz başlat (primeIdle/primeReady'deki allOff ve
// bank kapatma çağrılarını izole etmek için sayaçlar TEKRAR sıfırlanır).
void primeStopOrder() {
    s_stopTorqueCount = 0;
    s_stopTorqueFirstSeq = 0;
    s_stopLastTorque = 0xFFFF;
    fake_relay_reset();  // g_fake_call_seq / allOff_firstSeq = 0
    VcuLogic::setTorqueSink(stopTorqueSpy);
}

void primeReady() {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));
}

void primeDrive() {
    primeReady();
    VcuLogic::postEvent(VcuEvent::DRIVE_ENABLE);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::DRIVE),
                          static_cast<int>(VcuLogic::getState()));
}

}  // namespace

// --- Kabul edilen durumlar ------------------------------------------------

// READY + STOP -> IDLE (gecikmeli sira: 1. tick sifir tork, 2. tick acma)
void test_stop_from_ready_returns_to_idle(void) {
    primeReady();

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();  // (1) sifir tork — kontaktor HENUZ acilmadi, durum READY
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));

    VcuLogic::run();  // (2)+(3) gecikme doldu -> allOff + IDLE
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

// DRIVE + STOP -> IDLE
void test_stop_from_drive_returns_to_idle(void) {
    primeDrive();

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::DRIVE),
                          static_cast<int>(VcuLogic::getState()));

    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

// STOP kontaktorleri ACAR (allOff cagrilir) — IDLE'a donus "sessiz" degildir.
void test_stop_opens_contactors(void) {
    primeReady();
    const unsigned before = g_fake_relay_allOff_count;

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();
    // Ilk tick'te ACILMAZ — once tork sonmeli.
    TEST_ASSERT_EQUAL_UINT(before, g_fake_relay_allOff_count);

    VcuLogic::run();
    TEST_ASSERT_TRUE(g_fake_relay_allOff_count > before);
}

// ---------------------------------------------------------------------------
// P3 SIRA GARANTISI (asil regresyon): sifir-tork istegi ile kontaktor acma
// ARASINDA EN AZ BIR run() tick'i olmali. Duzeltmeden once STOP dalinda ikisi
// ARDISIK iki satirdi (gecikme HIC yoktu) — bu test onu yakalar.
// ---------------------------------------------------------------------------
void test_stop_zero_torque_precedes_contactor_open_by_a_tick(void) {
    primeReady();
    primeStopOrder();

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);

    // --- 1. tick: YALNIZ sifir tork ---
    VcuLogic::run();
    TEST_ASSERT_EQUAL_UINT(1, s_stopTorqueCount);
    TEST_ASSERT_EQUAL_UINT16(0, s_stopLastTorque);
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOff_count);  // ACMA YOK
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));

    // --- 2. tick: gecikme doldu -> acma ---
    VcuLogic::run();
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOff_count);
    // Tork istegi TEKRARLANMADI (STOP spam'i yok).
    TEST_ASSERT_EQUAL_UINT(1, s_stopTorqueCount);

    // SIRA: torque(0) kontaktor acmadan ONCE.
    TEST_ASSERT_TRUE(s_stopTorqueFirstSeq > 0);
    TEST_ASSERT_TRUE(g_fake_relay_allOff_firstSeq > 0);
    TEST_ASSERT_TRUE(s_stopTorqueFirstSeq < g_fake_relay_allOff_firstSeq);

    VcuLogic::setTorqueSink(nullptr);
}

// Bekleyen STOP varken gelen YINELENEN STOP zamanlayiciyi SIFIRLAMAMALI —
// aksi halde tekrar tekrar basmak kontaktor acmayi süresiz erteleyebilirdi.
void test_repeated_stop_does_not_postpone_contactor_open(void) {
    primeReady();
    primeStopOrder();

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();  // (1) sifir tork, bekleme basladi

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);  // yinelenen bas
    VcuLogic::run();  // gecikme yine de doldu -> acma

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOff_count);
    TEST_ASSERT_EQUAL_UINT(1, s_stopTorqueCount);  // ikinci istek yok sayildi

    VcuLogic::setTorqueSink(nullptr);
}

// Bekleyen STOP sirasinda E-STOP gelirse: E-STOP kazanir ve bayat STOP
// tamamlanip guvenlik durumunu IDLE'a DUSURMEZ.
void test_estop_during_pending_stop_wins_and_cancels_it(void) {
    primeReady();

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();  // STOP bekliyor (durum hala READY)

    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();  // E-STOP kazanir
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::EMERGENCY_STOP),
                          static_cast<int>(VcuLogic::getState()));

    // Bekleyen STOP iptal edildi: sonraki tick'ler IDLE'a DUSURMEZ.
    VcuLogic::run();
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::EMERGENCY_STOP),
                          static_cast<int>(VcuLogic::getState()));
}

// Acma ANINDA olmali: kademeli kapatma yolu (setBankStaggered) KULLANILMAZ.
// g_fake_relay_allOn_count hem allOn hem setBankStaggered'i sayar; STOP
// sirasinda ikisi de cagrilmamalidir.
void test_stop_does_not_use_staggered_bank_close(void) {
    primeReady();
    const unsigned before = g_fake_relay_allOn_count;

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();
    VcuLogic::run();

    TEST_ASSERT_EQUAL_UINT(before, g_fake_relay_allOn_count);
}

// --- Yok sayilan durumlar -------------------------------------------------

// IDLE + STOP -> IDLE'da kalir (yok sayilir, kontaktor durumu degismez).
void test_stop_in_idle_is_ignored(void) {
    primeIdle();
    const unsigned allOffBefore = g_fake_relay_allOff_count;

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(allOffBefore, g_fake_relay_allOff_count);
}

// FAULT + STOP -> FAULT'ta KALIR. STOP bir ariza durumunu TEMIZLEMEZ;
// fault'tan cikis yalniz RESET + interlock ile olur. Bu, guvenlik acisindan
// en kritik iddiadir: aksi halde ekrandaki bir buton fault'u atlatabilirdi.
void test_stop_in_fault_does_not_clear_fault(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::FAULT_DETECTED);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::FAULT),
                          static_cast<int>(VcuLogic::getState()));
}

// EMERGENCY_STOP + STOP -> EMERGENCY_STOP'ta KALIR. STOP, E-STOP'un yerini
// TUTMAZ ve onu DUSURMEZ (guvenlik durumu asla zayiflatilmaz).
void test_stop_in_estop_does_not_downgrade_safety_state(void) {
    primeIdle();
    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::EMERGENCY_STOP),
                          static_cast<int>(VcuLogic::getState()));

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::EMERGENCY_STOP),
                          static_cast<int>(VcuLogic::getState()));
}

// --- STOP sonrasi normal calisma devam eder -------------------------------

// STOP ile IDLE'a donduktan sonra START ile tekrar READY'ye gecilebilmeli
// (STOP bir ariza kaydi BIRAKMAZ — E-STOP'tan farki tam olarak budur).
void test_start_works_again_after_stop(void) {
    primeReady();
    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();  // (1) sifir tork
    VcuLogic::run();  // (2)+(3) acma + IDLE
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));

    VcuLogic::setTelemetryData(makeTelemetryDataValid());
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));
}

// E-STOP hala HER durumda calisir — STOP eklenmesi onu etkilemedi.
void test_estop_still_works_from_drive_after_stop_feature(void) {
    primeDrive();

    VcuLogic::postEvent(VcuEvent::EMERGENCY_STOP);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::EMERGENCY_STOP),
                          static_cast<int>(VcuLogic::getState()));
}
