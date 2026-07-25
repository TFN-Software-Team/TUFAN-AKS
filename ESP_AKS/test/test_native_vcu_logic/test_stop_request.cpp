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

namespace {

void primeIdle() {
    VcuLogic::resetForTest();
    fake_freertos_reset();
    fake_relay_reset();
    VcuLogic::init(g_mockRelay);
    VcuLogic::setTelemetryData(makeTelemetryDataValid());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
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

// READY + STOP -> IDLE
void test_stop_from_ready_returns_to_idle(void) {
    primeReady();

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

// DRIVE + STOP -> IDLE
void test_stop_from_drive_returns_to_idle(void) {
    primeDrive();

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
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

    TEST_ASSERT_TRUE(g_fake_relay_allOff_count > before);
}

// Acma ANINDA olmali: kademeli kapatma yolu (setBankStaggered) KULLANILMAZ.
// g_fake_relay_allOn_count hem allOn hem setBankStaggered'i sayar; STOP
// sirasinda ikisi de cagrilmamalidir.
void test_stop_does_not_use_staggered_bank_close(void) {
    primeReady();
    const unsigned before = g_fake_relay_allOn_count;

    VcuLogic::postEvent(VcuEvent::STOP_REQUEST);
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
    VcuLogic::run();
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
