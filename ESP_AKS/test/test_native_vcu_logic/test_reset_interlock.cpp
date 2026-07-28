#include <unity.h>

#include "VcuLogic.h"
#include "fake_freertos.h"
#include "mock_relay_actuator.h"
#include "test_helpers.h"

using test_helpers::makeTelemetryDataValid;
using VcuLogic::isResetInterlockSatisfied;
using VcuLogic::VcuEvent;
using VcuLogic::VcuState;

// ---------------------------------------------------------------------------
// Reset interlock — FAULT/EMERGENCY_STOP'tan IDLE'ya geçiş için ön koşul.
// hasCriticalCondition false olmalı; motor kaynaklı ek şartlar YALNIZCA
// MOTOR_DRIVER_PRESENT=1 iken aranır (bkz. VcuLogic.h'deki #if blokları).
// Bu binary bayrağı 0 ile derler — flag=1 dalı test_native_ready_motor'dadır.
// ---------------------------------------------------------------------------
void test_reset_interlock_clean_baseline_passes(void) {
    TelemetryData d = makeTelemetryDataValid();
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::EMERGENCY_STOP));
}

// MOTOR_DRIVER_PRESENT=0 iken 0x200'ü hall-effect hız sensörü üretiyor ve
// data[7] bit anlamları motor sürücüsü için DOĞRULANMAMIŞ — doğrulanmamış bir
// sinyal FAULT'tan çıkışı BLOKLAMAMALI (CLAUDE.md Kural 4 / Ek B).
void test_reset_interlock_motor_error_does_not_block_when_flag0(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorErrorFlags = 0x02;
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::EMERGENCY_STOP));
}

void test_reset_interlock_bms_error_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsSystemState = 4;  // FAULT (devre disi: AKS-17)
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_unverified_bms_system_state_does_not_block(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsSystemState = 4;  // FAULT shaped output
    d.TEL_bmsDataValid = false;
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// Sıcaklık artık karar mantığına BAĞLI (≥70 °C kritik): kritik sıcaklık
// sürerken reset REDDEDİLMELİ; yalnız WARN bandındaki (55–69 °C) sıcaklık
// kritik olmadığı için reset'i bloklamaz.
void test_reset_interlock_critical_temp_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 75;  // ≥70 °C — kritik, FAULT'tan çıkışı engeller
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_warning_temp_does_not_block(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTempHighestC = 60;  // WARN bandı — kritik değil, reset serbest
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// Akım artık karar mantığına BAĞLI: kritik akım (deşarj ≥15 A / şarj ≥13 A)
// sürerken reset REDDEDİLMELİ; nominal şarj akımı (9.9 A saha gözlemi)
// reset'i bloklamaz.
void test_reset_interlock_critical_current_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = -2000;  // 20 A deşarj — kritik, reset reddedilir
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_nominal_charge_current_does_not_block(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsCurrentCentiA = 990;  // 9.9 A şarj — eşiklerin altında, serbest
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_critical_voltage_low_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 590;  // < 600 dV critical (24S LiFePO4 spec min)
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_critical_voltage_high_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 900;  // > 876 dV critical (spec maks)
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// REGRESYON (sahada gözlenen kilitlenme): MOTOR_DRIVER_PRESENT=0 iken motor
// timeout'u FAULT'tan çıkışı BLOKLAMAMALI. Eskiden hasCriticalCondition bunu
// kritik saydığı için hall sensörü sustuğunda araç FAULT'ta KALICI kilitleniyor
// (RESET her seferinde reddediliyor, otomatik reset de takılıyordu).
void test_reset_interlock_motor_timeout_does_not_block_when_flag0(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// BMS timeout da motor timeout ile aynı şekilde reset'i bloklar.
void test_reset_interlock_bms_timeout_in_fault_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsTimeoutActive = true;
    d.TEL_bmsDataValid = false;
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// Sadece warning seviyesindeki bir koşul reset'i bloklamamalı.
void test_reset_interlock_warning_level_passes(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_bmsPackVoltageDeciV = 720;  // warn eşiğinde ama critical değil
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// AKS-04: Hareket halinde RESET'e izin verme
void test_reset_interlock_rpm_zero_fresh_passes(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorRpm = 0;
    d.TEL_motorDataValid = true;
    d.TEL_motorTimeoutActive = false;
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_rpm_high_blocks(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorRpm = 500;
    d.TEL_motorDataValid = true;
    d.TEL_motorTimeoutActive = false;
    TEST_ASSERT_FALSE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

void test_reset_interlock_rpm_noise_passes(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorRpm = -30; // |30| < 50
    d.TEL_motorDataValid = true;
    d.TEL_motorTimeoutActive = false;
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// ---------------------------------------------------------------------------
// MOTOR_DRIVER_PRESENT=0 — hall-effect hız sensörü akışının kesilmesi
// ---------------------------------------------------------------------------
// ASIL REGRESYON: sensör sustuğunda CanManager motorDataValid=false VE
// motorTimeoutActive=true yapar. Bayrak 0 iken bu, aracın FAULT/EMERGENCY_STOP'ta
// KALICI kilitlenmesine yol açıyordu. Artık her iki durumdan da çıkış serbest.
void test_reset_interlock_motor_stream_lost_passes_when_flag0(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorDataValid = false;
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::EMERGENCY_STOP));
}

// BİLİNÇLİ TAVİZ: freshness kaybında CanManager TEL_motorRpm'i SIFIRLAMAZ —
// son değer donar. RPM kontrolü bu yüzden yalnız TEL_motorDataValid iken
// çalışır; aksi halde donmuş bir RPM reset'i SONSUZA KADAR bloklardı (yeni
// frame gelmediği için değer asla düşmez). Hareket halinde reset yasağı, veri
// TAZE olduğu sürece (bugün hall sensöründen) aynen sürer —
// bkz. test_reset_interlock_rpm_high_blocks.
void test_reset_interlock_stale_high_rpm_is_ignored_when_flag0(void) {
    TelemetryData d = makeTelemetryDataValid();
    d.TEL_motorRpm = 500;             // son görülen (donmuş) değer
    d.TEL_motorDataValid = false;     // taze değil → RPM'e bakılmaz
    d.TEL_motorTimeoutActive = true;
    TEST_ASSERT_TRUE(isResetInterlockSatisfied(d, VcuState::FAULT));
}

// ---------------------------------------------------------------------------
// IDLE'da RESET — latch'lenmiş actuator fault için "TEKRAR DENE" yolu
// ---------------------------------------------------------------------------
// Bu blok saf predicate DEĞİL, durum makinesi (run()) davranışını doğrular:
// RelayManager bir geri-okuma uyuşmazlığında actuator fault'u LATCH'ler ve bu
// IDLE'da START'ı KALICI olarak reddettiriyordu — clearActuatorFault() yalnız
// FAULT/E-STOP'tan çıkışta çağrıldığı için tek çıkış reboot'tu.
namespace {

// test_state_machine.cpp'deki primeIdle ile aynı kurulum (statik state, queue
// ve röle sayaçları sıfırlanır, init() ile IDLE'ye geçilir). Dosya-yerel
// tutuldu: iki .cpp aynı binary'de linklendiğinden isim çakışması olmasın.
void primeIdleForResetTests() {
    VcuLogic::resetForTest();
    fake_freertos_reset();
    fake_relay_reset();
    VcuLogic::init(g_mockRelay);
    VcuLogic::setTelemetryData(makeTelemetryDataValid());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
}

}  // namespace

void test_idle_reset_clears_actuator_fault_and_stays_idle(void) {
    primeIdleForResetTests();

    // Röle geri-okuma uyuşmazlığı latch'lendi → START reddediliyor.
    g_fake_relay_actuatorFault = true;
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOn_count);

    // IDLE'da RESET: durum DEĞİŞMEZ, yalnız fault temizlenir.
    const unsigned clearsBefore = g_fake_relay_clearFault_count;
    VcuLogic::postEvent(VcuEvent::RESET);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_UINT(clearsBefore + 1, g_fake_relay_clearFault_count);
    TEST_ASSERT_FALSE(g_fake_relay_actuatorFault);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    // Fault temizlemek kontaktörlere DOKUNMAZ (bank kapatma yolu çağrılmadı).
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOn_count);

    // Ve asıl mesele: START artık kabul ediliyor (reboot gerekmiyor).
    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::READY),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(1, g_fake_relay_allOn_count);
}

// BYPASS DEĞİL: donanım hâlâ bozuksa bir sonraki verify taraması fault'u
// YENİDEN latch'ler ve START tekrar bloklanır. (Mock'ta "kalıcı bozukluk",
// temizlemeden sonra bayrağın yeniden set edilmesiyle taklit edilir — gerçek
// RelayManager'da bunu periyodik verifyIfDue/verifyOutputs yapar.)
void test_idle_reset_is_not_a_bypass_when_hardware_still_broken(void) {
    primeIdleForResetTests();

    g_fake_relay_actuatorFault = true;
    VcuLogic::postEvent(VcuEvent::RESET);
    VcuLogic::run();
    TEST_ASSERT_FALSE(g_fake_relay_actuatorFault);  // temizlendi

    g_fake_relay_actuatorFault = true;  // donanım hâlâ bozuk → yeniden latch

    VcuLogic::postEvent(VcuEvent::START_REQUEST);
    VcuLogic::run();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(VcuState::IDLE),
                          static_cast<int>(VcuLogic::getState()));
    TEST_ASSERT_EQUAL_UINT(0, g_fake_relay_allOn_count);
}
