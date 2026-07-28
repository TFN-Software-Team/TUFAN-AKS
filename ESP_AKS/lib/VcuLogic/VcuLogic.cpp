#include <atomic>
#include <cmath>

#include "VcuLogic.h"
#include "DeratingPolicy.h"
#include "HeadlightSwitch.h"
#include "IRelayActuator.h"
#include "SystemConfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#ifndef NATIVE_BUILD
#include "esp_timer.h"   // AKS-10: gercek ms tabani (esp_timer_get_time)
#endif

static constexpr const char* TAG = "VCU_LOGIC";
static constexpr uint32_t TASK_PERIOD_MS = 20;

// NOT (28.07.2026): burada bir `#if MOTOR_DRIVER_PRESENT #warning` vardı —
// STOP_REQUEST yolunun sıfır-tork ile kontaktör açma arasında
// VCU_CONTACTOR_OPEN_DELAY_MS BEKLEMEDİĞİNİ söylüyordu. Açık KAPANDI: STOP
// artık E-STOP/FAULT ile aynı gecikmeli sırayı izliyor (bkz. run() içindeki
// "bekleyen STOP" bloğu), bu yüzden uyarı kaldırıldı.

namespace VcuLogic {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static std::atomic<VcuState> s_state{VcuState::INIT};
static QueueHandle_t s_eventQueue = nullptr;
static uint32_t s_stateTimer = 0;
// Monotonic uptime (transition'da SIFIRLANMAZ) — aktüatör periyodik verify'ının
// zamanlaması için (s_stateTimer state geçişinde sıfırlandığından periyot
// ölçümüne uygun değil).
static uint32_t s_uptimeMs = 0;
static uint32_t s_lastTimeMs = 0;
static TelemetryData s_TEL_latestData = {};
static bool s_VCU_warningLogged = false;
static SemaphoreHandle_t s_TEL_dataMutex = nullptr;
// E-STOP bypass: set atomically in postEvent so queue saturation
// cannot swallow an emergency stop command.
static std::atomic<bool> s_eStopPending{false};

// R1: FAULT bypass — E-STOP ile AYNI desen. FAULT_DETECTED kuyruğu bypass edip
// yalnız bu atomic bayrağı set eder; kuyruk dolu olsa bile fault KAYBOLMAZ.
// TEMİZLENME KURALI: run() her tick exchange(false) ile okur→tüketir; yani
// bayrak "işlenmemiş fault isteği var mı"yı temsil eder ve okunduğu an temizlenir.
// İstek, FAULT'a geçilerek (veya zaten FAULT olunduğu için ek geçiş gerekmeden)
// handle edilmiş sayılır.
static std::atomic<bool> s_faultPending{false};

// M2: enjekte edilen aktüatör (röle sürücüsü) arayüzü. init() içinde bağlanır;
// somut RelayManager singleton'ına doğrudan bağımlılık YOK.
static IRelayActuator* s_relays = nullptr;

static bool s_relaysOpenedInEstop = false;
static bool s_relaysOpenedInFault = false;
static uint32_t s_lastEstopLogMs = 0;
static uint32_t s_lastFaultLogMs = 0;
static uint32_t s_autoResetTimer = 0;

// ---------------------------------------------------------------------------
// P3 GÜVENLİ KAPANIŞ SIRASI — sıfır-tork ile kontaktör açma arasındaki gecikme
// ---------------------------------------------------------------------------
// Sıra: (1) sıfır tork iste → (2) VCU_CONTACTOR_OPEN_DELAY_MS bekle → (3) aç.
// (1) ve (3) ASLA aynı tick'te olmamalıdır; olursa tork sönmeye fırsat bulamaz
// ve kontaktörler YÜK ALTINDA açılır (ark / kontak kaynaması / regen aşırı
// gerilimi).
//
// ESKİ HATA (düzeltildi): adım (1) handler'ın "ilk tick" guard'ına
// (s_stateTimer <= TASK_PERIOD_MS) bağlıydı, adım (3) ise
// (s_stateTimer >= VCU_CONTACTOR_OPEN_DELAY_MS) koşuluna. transitionTo sonrası
// run() RETURN ettiği için handler ilk kez s_stateTimer==TASK_PERIOD_MS ile
// çalışıyordu ve VCU_CONTACTOR_OPEN_DELAY_MS == TASK_PERIOD_MS == 20 olduğundan
// İKİ KOŞUL DA AYNI TICK'TE sağlanıyordu — yani gecikme fiilen SIFIRDI.
//
// ÇÖZÜM: adım (1) artık handler'da değil, GEÇİŞİN KENDİSİNDE (transitionTo →
// beginSafeShutdown) t=0'da yapılır ve zaman damgası kaydedilir. Adım (3) ise
// "damgadan bu yana >= VCU_CONTACTOR_OPEN_DELAY_MS geçti mi" sorusuna bağlıdır.
// Aşağıdaki static_assert, gecikmenin en az bir tik olmasını garanti eder —
// dolayısıyla (3) her zaman (1)'den EN AZ bir tick SONRA çalışır.
static_assert(VCU_CONTACTOR_OPEN_DELAY_MS >= TASK_PERIOD_MS,
              "VCU_CONTACTOR_OPEN_DELAY_MS en az bir VCU tik periyodu olmali; "
              "aksi halde sifir-tork ve kontaktor acma AYNI tick'e duser ve "
              "kontaktorler yuk altinda acilir (bkz. MOTOR_ENTEGRASYON_NOTU.md).");

// Bu güvenli-kapanış epizodunda sıfır tork istendi mi + hangi uptime damgasında.
// s_stateTimer DEĞİL s_uptimeMs kullanılır: s_stateTimer geçişte sıfırlandığı
// için, kapanış sırası devam ederken araya giren bir geçiş (ör. FAULT → E-STOP)
// ölçümü bozardı.
static bool s_zeroTorqueSent = false;
static uint32_t s_zeroTorqueAtMs = 0;

// BÖLÜM C: bekleyen kontrollü durdurma (ekran "DUR" / STOP_REQUEST). STOP da
// aynı güvenli kapanış sırasını izler; kontaktör açma + IDLE dönüşü bir sonraki
// tick'e ertelenir. VCU task'i vTaskDelay ile BLOKLANMAZ (tick sayarak beklenir).
static bool s_stopPendingOpen = false;

// READY girişi reddedildiğinde log spam'ini önlemek için: aynı ret nedeni her
// tick değil, neden değiştiğinde veya en fazla 1 sn'de bir loglanır. Neden
// string'leri statik literal olduğundan pointer karşılaştırması geçerlidir.
static const char* s_lastReadyRejectReason = nullptr;
static uint32_t s_lastReadyRejectLogMs = 0;

// Torque komut sink'i (main.cpp bağlar). Motor sürücüsü entegrasyon iskeleti;
// bağlı değilse istek sessizce yok sayılır.
static TorqueSink s_torqueSink = nullptr;

#if RELAY_ROLES_ASSIGNED
// Flaşör gölge durumu (şartname 6.e.ii) — kenar-tetikli setRelay için son
// komut edilen durum. Karar mantığı saf flasherDesiredState'te (VcuLogic.h);
// FAULT/E-STOP dahil HER tick çalışır (allOff bank maskesini açtığından
// flaşör kanalına dokunmaz — sıcaklık eşikte kaldıkça yanık kalır).
static bool s_flasherOn = false;

// IDLE'daki S1 (şarj hattı kontaktörü) son komutu: -1 = bilinmiyor (IDLE'a
// her girişte sıfırlanır → ilk IDLE tick'i S1'i deterministik olarak charger
// durumuna göre yazar), 0/1 = son setRelay komutu. Kenar-tetikli: her tick
// SPI yazmamak için yalnız istenen durum değişince setRelay çağrılır.
static int8_t s_s1LastCmdInIdle = -1;

// Fan gölge durumu (şartname B3 7.a-b) — flaşörün ikizi. Karar saf
// fanDesiredState'te (VcuLogic.h); FAULT/E-STOP dahil HER tick çalışır
// (fan kanalı bank maskesi DIŞINDA → allOff dokunmaz). Kenar-tetikli setRelay.
static bool s_fanOn = false;

// Far gölge durumu (şartname B2 9.19.c) — boot'ta OFF. ARTIK ekran komutuyla
// değil, fiziksel düğmeyle sürülür (s_headlightSwitchReader ile okunur, saf
// karar HeadlightSwitch::update). s_headlightOn, run()'ın (VCU task) far
// rölesine yazdığı SON komuttur (kenar-tetikli setRelay için gölge) ve HMI
// task tarafından isHeadlightOn() ile OKUNUR (far.pic göstergesi) → cross-task
// erişim, atomic. Düğme okuma yolu SPI'dan bağımsızdır (doğrudan GPIO reader).
static std::atomic<bool> s_headlightOn{false};
static HeadlightSwitch::State s_headlightSwitch = {};
static VcuLogic::HeadlightSwitchReader s_headlightSwitchReader = nullptr;
#endif

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void transitionTo(VcuState next);
static void requestZeroTorque();
static void beginSafeShutdown();
static bool contactorOpenDelayElapsed();
static bool pollEvent(VcuEvent& out);
static bool isResetInterlockSatisfied();
static bool hasWarningCondition();
static bool hasCriticalCondition();
static const char* readyRejectReason(const TelemetryData& VCU_data);
static TelemetryData getTelemetrySnapshot();

static void handleInit();
static void handleIdle();
static void handleReady();
static void handleDrive();
static void handleEmergencyStop();
static void handleFault();

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void init(IRelayActuator& relays) {
    s_relays = &relays;

    // Safety first — ensure all relays are off at startup, even if init fails
    s_relays->allOff(false);

    s_eventQueue = xQueueCreate(8, sizeof(VcuEvent));
    if (s_eventQueue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        s_relays->allOff(false);
        return;
    }

    s_TEL_dataMutex = xSemaphoreCreateMutex();
    if (s_TEL_dataMutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create telemetry mutex");
        s_relays->allOff(false);
        return;
    }

#ifdef NATIVE_BUILD
    s_lastTimeMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
#else
    s_lastTimeMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
#endif
    transitionTo(VcuState::IDLE);
}

#ifdef NATIVE_BUILD
extern "C" void fake_freertos_advance_time(uint32_t);
#endif

void run() {
#ifdef NATIVE_BUILD
    fake_freertos_advance_time(TASK_PERIOD_MS);
    // Native testlerde esp_timer_get_time yok — tik sayacı ms tabanli
    // (portTICK_PERIOD_MS=1) oldugundan xTaskGetTickCount() zaten ms verir.
    uint32_t nowMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
#else
    // AKS-10: Gercek ms tabani — esp_timer_get_time() mikrosaniye verir,
    // /1000 ile ms'e cevirilir. xTaskGetTickCount()*portTICK_PERIOD_MS
    // yalnizca 10ms cozunurlugu (100 Hz tik) sagliyordu; bu, 20ms VCU
    // dongusu icin yeterli ama zamanlamaya bagli kararlarda drift riski
    // tasiyordu. Isaresiz cikarma idiomu uint32_t sarmasi ile dogru calisir.
    uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
#endif
    uint32_t delta = nowMs - s_lastTimeMs;
    s_lastTimeMs = nowMs;

    s_stateTimer += delta;
    s_uptimeMs += delta;

#if RELAY_ROLES_ASSIGNED
    // Flaşör (şartname 6.e.ii): sıcaklık uyarısına bağlı sesli+ışıklı ikaz.
    // run()'ın EN BAŞINDA, tüm erken-return'lardan ÖNCE — FAULT/E-STOP dahil
    // her durumda ve her tick'te çalışır. Karar saf flasherDesiredState'te:
    // 55 °C'de ON, (55−FLASHER_HYSTERESIS_C)=53 °C altına inince OFF; bayat
    // veri/timeout'ta son durum korunur. handleFault/handleEmergencyStop'un
    // periyodik allOff re-assert'i bank maskesi dışındaki flaşör kanalına
    // dokunmadığından bu mantıkla çakışmaz.
    {
        const TelemetryData VCU_snap = getTelemetrySnapshot();
        const bool desired = flasherDesiredState(
            s_flasherOn, VCU_snap.TEL_bmsDataValid,
            VCU_snap.TEL_bmsTimeoutActive, VCU_snap.TEL_bmsTempHighestC);
        if (desired != s_flasherOn) {
            s_relays->setRelay(RELAY_CH_FLASHER, desired);
            ESP_LOGW(TAG, "Sicaklik uyari flasoru %s (temp=%d C)",
                     desired ? "YANDI" : "SONDU",
                     (int)VCU_snap.TEL_bmsTempHighestC);
            s_flasherOn = desired;
        }

        // Soğutma fanı (şartname B3 7.a-b) — flaşörle AYNI desen ve AYNI
        // snapshot: 40 °C'de ON, 35 °C'ye inince OFF; bayat veri/BMS timeout'ta
        // dokunma. FAULT/E-STOP dahil her tick çalışır (bank dışı kanal).
        const bool fanDesired = fanDesiredState(
            s_fanOn, VCU_snap.TEL_bmsDataValid,
            VCU_snap.TEL_bmsTimeoutActive, VCU_snap.TEL_bmsTempHighestC);
        if (fanDesired != s_fanOn) {
            s_relays->setRelay(RELAY_CH_FAN, fanDesired);
            ESP_LOGI(TAG, "Sogutma fani %s (temp=%d C)",
                     fanDesired ? "ACILDI" : "KAPANDI",
                     (int)VCU_snap.TEL_bmsTempHighestC);
            s_fanOn = fanDesired;
        }
    }

    // Far (şartname B2 9.19.c): fiziksel düğmeyi oku (debounce + latching/
    // momentary, saf karar HeadlightSwitch::update) ve far rölesini kenar-
    // tetikli sür. BMS'ten BAĞIMSIZ; far kanalı bank maskesi DIŞINDA →
    // allOff/allOn (FAULT/E-STOP/READY) far durumunu DEĞİŞTİRMEZ. s_uptimeMs
    // monoton zaman damgası olarak kullanılır (debounce). Reader bağlı değilse
    // (üretimde bayrak=1'de main.cpp bağlar; testler spy takar) far OFF kalır.
    if (s_headlightSwitchReader != nullptr) {
        const int hlRaw = s_headlightSwitchReader();
        const bool hlDesired = HeadlightSwitch::update(
            s_headlightSwitch, hlRaw, s_uptimeMs, HEADLIGHT_SWITCH_LATCHING,
            HEADLIGHT_DEBOUNCE_MS);
        if (hlDesired != s_headlightOn.load(std::memory_order_relaxed)) {
            s_relays->setRelay(RELAY_CH_HEADLIGHT, hlDesired);
            ESP_LOGI(TAG, "Far %s (fiziksel dugme)",
                     hlDesired ? "ACILDI" : "KAPANDI");
            s_headlightOn.store(hlDesired, std::memory_order_relaxed);
        }
    }
#endif

    if (s_eStopPending.exchange(false, std::memory_order_acquire)) {
        if (s_state.load(std::memory_order_relaxed) != VcuState::EMERGENCY_STOP) {
            transitionTo(VcuState::EMERGENCY_STOP);
            return;
        }
        // Already in EMERGENCY_STOP — clear flag but let run() continue
        // so handleEmergencyStop() keeps executing (timer must not reset).
    }

    // R1: FAULT bypass — kuyruk dolu olsa bile kaybolmayan fault isteği. E-STOP'tan
    // SONRA kontrol edilir (E-STOP daha yüksek öncelikli güvenli durum). Zaten
    // FAULT ise re-entry yok (timer sıfırlanmaz); değilse FAULT'a geç. Bayrak her
    // durumda exchange ile tüketilir (temizlenme kuralı — bkz. tanım).
    if (s_faultPending.exchange(false, std::memory_order_acquire)) {
        VcuState currentSt = s_state.load(std::memory_order_relaxed);
        if (currentSt == VcuState::EMERGENCY_STOP) {
            ESP_LOGE(TAG, "Fault detected during E-STOP. Fault registered.");
            // Guvenlik durumu ASLA dusurulmez: E-STOP > FAULT (AKS-05)
        } else if (currentSt != VcuState::FAULT) {
            ESP_LOGE(TAG, "FAULT pending (atomic bypass) — entering FAULT");
            transitionTo(VcuState::FAULT);
            return;
        }
    }

    // G3: hafif periyodik actuator (röle çıkışı) doğrulaması. Aktüatör katmanı
    // RELAY_VERIFY_PERIOD_MS'den seyrek olmayacak şekilde OLAT/IODIR'i geri
    // okur; uyuşmazlıkta re-init + re-assert eder ve kalıcı atomic fault
    // bayrağını set eder.
    s_relays->verifyIfDue(s_uptimeMs);

    // Actuator fault kalıcı bayrağını HER tick oku (R1: düşen event tuzağı
    // yok). HV bus canlıyken (READY/DRIVE) gelen fault, mevcut fault yoluna
    // (E-STOP dizisi / P3 sıralı kontaktör açma) girer. IDLE'da ise READY
    // giriş guard'ı ile bloklanır (aşağıda), zorla FAULT'a geçilmez.
    {
        VcuState st = s_state.load(std::memory_order_relaxed);
        if ((st == VcuState::READY || st == VcuState::DRIVE) &&
            s_relays->hasActuatorFault()) {
            ESP_LOGE(TAG, "Actuator fault while HV live — entering FAULT");
            transitionTo(VcuState::FAULT);
            return;
        }
    }

    VcuState currentState = s_state.load(std::memory_order_relaxed);
    if ((currentState == VcuState::READY || currentState == VcuState::DRIVE) &&
        hasCriticalCondition()) {
        ESP_LOGE(TAG, "Critical safety threshold exceeded, entering FAULT");
        transitionTo(VcuState::FAULT);
        return;
    }

    // BÖLÜM C — bekleyen kontrollü durdurmanın (STOP) 2.+3. adımı.
    // STOP_REQUEST dalı sıfır torku istedi ve buraya bıraktı; kontaktör açma
    // ile IDLE dönüşü, torkun sönmesi için EN AZ VCU_CONTACTOR_OPEN_DELAY_MS
    // beklendikten sonra yapılır. Bu blok E-STOP/FAULT/aktüatör-fault
    // kontrollerinden SONRA gelir: onlar durumu değiştirirse (transitionTo
    // s_stopPendingOpen'ı temizler) bekleyen STOP zaten iptal olmuştur ve
    // güvenlik durumu IDLE'a düşürülmez.
    if (s_stopPendingOpen) {
        if (currentState != VcuState::READY && currentState != VcuState::DRIVE) {
            // Savunma amaçlı: transitionTo zaten iptal ediyor, buraya
            // düşülmemeli. Yine de bayat bir STOP'un kontaktör açmasına izin
            // verme (durum artık STOP'un anlamlı olduğu bir durum değil).
            s_stopPendingOpen = false;
        } else if (contactorOpenDelayElapsed()) {
            // Açma her zaman ANINDA olmalıdır — setBankStaggered (kademeli
            // KAPATMA, inrush içindir) BURADA KULLANILMAZ. allOff bank
            // maskesini (S1 + S2 + sürüş bankı) açar; flaşör/fan/far maske
            // DIŞINDA olduğu için etkilenmez (FAULT/E-STOP ile aynı davranış).
            s_relays->allOff(false);
            s_stopPendingOpen = false;

            // transitionTo(IDLE) s_s1LastCmdInIdle'ı "bilinmiyor" yapar; bir
            // sonraki handleIdle tick'i S1'i chargerActive durumuna göre
            // deterministik olarak yeniden yazar (şartname 8.2.a.iii).
            ESP_LOGI(TAG, "STOP: kontrollu durdurma (%s -> IDLE)",
                     currentState == VcuState::DRIVE ? "DRIVE" : "READY");
            transitionTo(VcuState::IDLE);
            return;
        }
    }

    // AÇIK İŞ (B12): Warning bandında derating (tork/güç sınırlama) politikası
    // — İSKELET KURULDU (bkz. lib/VcuLogic/DeratingPolicy.h). WARN aktifken
    // 0..100 bir tork-izin yüzdesi hesaplanıp kenar-tetikli loglanır, ama
    // ARAÇ DAVRANIŞI HALA DEĞİŞMEZ: bu yüzde hiçbir tork komutuna/CanManager
    // çağrısına bağlanmıyor (motor sürücüsü elimizde yok — bkz. KAPSAM KİLİDİ,
    // DeratingPolicy.h başlık yorumu). Warning READY girişini
    // isReadyEntryPermitted üzerinden zaten bloklar.
    //
    // ENTEGRASYON NOKTASI (motor sürücüsü geldiğinde): aşağıdaki
    // `deratingPercent` değeri, tork komut yolu (setTorqueSink/G2) gerçek
    // frame üretmeye başladığında torku sınırlamak için BURADA kullanılacak
    // (ör. VcuLogic'in DRIVE'da hesapladığı hedef torku bu yüzdeyle çarpıp
    // sink'e onu göndermek) — bugün böyle bir tork komut ÜRETİMİ yok, bu
    // yüzden bağlanacak bir şey de yok. Bkz. SystemConfig.h "B12: Derating
    // Policy" notu (kademe değerleri CONFIG, ekip kalibrasyonu bekliyor).
    if (hasWarningCondition()) {
        if (!s_VCU_warningLogged) {
            const uint8_t deratingPercent =
                DeratingPolicy::computeTorqueAllowPercent(getTelemetrySnapshot());
            ESP_LOGW(TAG,
                     "Warning threshold active, derating onerisi %%%u "
                     "(tork komutuna henuz baglanmadi — motor surucusu yok)",
                     (unsigned)deratingPercent);
            s_VCU_warningLogged = true;
        }
    } else {
        s_VCU_warningLogged = false;
    }

    VcuEvent event = VcuEvent::NONE;
    if (pollEvent(event)) {
        // High priority events — handled regardless of current state
        if (event == VcuEvent::EMERGENCY_STOP) {
            transitionTo(VcuState::EMERGENCY_STOP);
            return;
        }
        if (event == VcuEvent::FAULT_DETECTED) {
            // R1: FAULT normalde atomic bayrak yoluyla (kuyruk bypass) işlenir;
            // bu dal savunma amaçlı fallback'tir. Zaten FAULT ise re-entry yok.
            VcuState currentSt = s_state.load(std::memory_order_relaxed);
            if (currentSt == VcuState::EMERGENCY_STOP) {
                ESP_LOGE(TAG, "Fault detected during E-STOP. Fault registered.");
                // Guvenlik durumu ASLA dusurulmez: E-STOP > FAULT (AKS-05)
            } else if (currentSt != VcuState::FAULT) {
                transitionTo(VcuState::FAULT);
            }
            return;
        }

        currentState = s_state.load(std::memory_order_relaxed);
        if (event == VcuEvent::RESET &&
            (currentState == VcuState::FAULT ||
             currentState == VcuState::EMERGENCY_STOP ||
             currentState == VcuState::IDLE)) {

            // IDLE'daki RESET, FAULT/E-STOP'takinden FARKLI bir iştir: bir
            // DURUM GEÇİŞİ değil, latch'lenmiş aktüatör fault'u için
            // "TEKRAR DENE" yoludur.
            //
            // NEDEN GEREKLİ: RelayManager bir geri-okuma uyuşmazlığında
            // actuator fault'u LATCH'ler. Bu fault IDLE'da START'ı kalıcı
            // olarak reddettiriyordu ("READY gecisi reddedildi: actuator
            // fault") ve clearActuatorFault() yalnız FAULT/E-STOP'tan çıkışta
            // çağrıldığı için IDLE'da hiçbir ekran komutuyla temizlenemiyordu
            // — tek çıkış reboot'tu.
            //
            // NEDEN BYPASS DEĞİL: temizleme kalıcı değildir. Donanım gerçekten
            // bozuksa bir sonraki periyodik verifyIfDue taraması fault'u
            // YENİDEN latch'ler ve START tekrar bloklanır. Yani bu yol, geçici
            // bir uyuşmazlıktan (ör. tek seferlik SPI gürültüsü) sonra sürücüye
            // güvenli bir "tekrar dene" imkânı verir; bozuk donanımı gizlemez.
            //
            // NEDEN INTERLOCK ARANMAZ: isResetInterlockSatisfied()'in amacı
            // CANLI HV bus'tan (FAULT/E-STOP) çıkışı korumaktır. IDLE'da
            // kontaktörler zaten AÇIK ve bus ÖLÜ — korunacak bir geçiş yok.
            //
            // NEDEN transitionTo ÇAĞRILMAZ: zaten IDLE'dayız. transitionTo
            // s_stateTimer'ı sıfırlardı (READY-reddi log kısıtlaması buna
            // dayanıyor) ve RELAY_ROLES_ASSIGNED=1 iken s_s1LastCmdInIdle'ı
            // "bilinmiyor" yapardı — bu da bir sonraki handleIdle tick'inde
            // S1'in gereksiz yere yeniden yazılmasına yol açardı (şartname
            // 8.2.a.iii yolu).
            if (currentState == VcuState::IDLE) {
                s_relays->clearActuatorFault();
                ESP_LOGI(TAG, "IDLE: actuator fault temizlendi (RESET)");
                return;
            }

            if (!isResetInterlockSatisfied()) {
                TelemetryData VCU_snap = getTelemetrySnapshot();
                ESP_LOGW("VCU", "RESET reddedildi: rpm=%d, timeout=%d",
                         std::abs(VCU_snap.TEL_motorRpm), VCU_snap.TEL_motorTimeoutActive);
                return;
            }
            // Actuator fault'u temizle; donanım hâlâ bozuksa bir sonraki
            // periyodik verify yeniden latch'ler ve READY tekrar bloklanır.
            s_relays->clearActuatorFault();
            transitionTo(VcuState::IDLE);
            return;
        }

#if RELAY_ROLES_ASSIGNED
        if (event == VcuEvent::HEADLIGHT_TOGGLE) {
            bool hlDesired = !s_headlightOn.load(std::memory_order_relaxed);
            s_relays->setRelay(RELAY_CH_HEADLIGHT, hlDesired);
            ESP_LOGI(TAG, "Far %s (ekran uzerinden toggle)", hlDesired ? "ACILDI" : "KAPANDI");
            s_headlightOn.store(hlDesired, std::memory_order_relaxed);
            // return yok, asagidaki isleme devam et
        }
#endif

        // KONTROLLÜ DURDURMA (ekran "DUR" butonu — HMI_CMD_STOP).
        //
        // E-STOP'un YERİNİ TUTMAZ: E-STOP yukarıda, kuyruğu bypass eden atomic
        // bayrak yoluyla ve HER durumda işlenir; bu dal yalnız READY/DRIVE'da
        // anlamlıdır ve bir arıza kaydı BIRAKMAZ. Diğer tüm durumlarda
        // (INIT/IDLE/FAULT/EMERGENCY_STOP) YOK SAYILIR — özellikle FAULT'u
        // TEMİZLEMEZ (fault'tan çıkış yalnız RESET + interlock ile olur).
        //
        // HIZ KONTROLÜ YOK (bilinçli karar): RESET'in aksine STOP, araç
        // hareket halindeyken de kabul edilir. Gerekçe: sürücü bilinçli olarak
        // durduruyor; hız şartı koymak, tam da durdurulması istenen anda
        // komutu reddederdi. (RESET'te VCU_RESET_MAX_RPM şartı vardır çünkü o,
        // bir arızadan ÇIKIŞTIR — farklı bir güvenlik sorusudur.)
        if (event == VcuEvent::STOP_REQUEST) {
            if (currentState == VcuState::READY || currentState == VcuState::DRIVE) {
                // STOP da E-STOP/FAULT ile AYNI güvenli kapanış sırasını izler:
                //   (1) sıfır tork iste → (2) VCU_CONTACTOR_OPEN_DELAY_MS
                //   bekle → (3) kontaktör aç + IDLE'a dön.
                // Burada YALNIZ (1) yapılır; (2)+(3) run() başındaki "bekleyen
                // STOP" bloğunda, EN AZ bir tick SONRA tamamlanır.
                //
                // NEDEN vTaskDelay DEĞİL: beklemeyi bloklayarak yapmak VCU
                // task'ini durdururdu — E-STOP bayrağı, flaşör/fan mantığı ve
                // aktüatör doğrulaması da o süre boyunca işlemezdi. Bekleme
                // tick sayarak yapılır (s_uptimeMs damgası).
                //
                // ESKİ HATA (düzeltildi): burada requestZeroTorque() ile
                // allOff() ARDIŞIK çağrılıyordu — yani gecikme HİÇ yoktu.
                // Dosya başındaki #if MOTOR_DRIVER_PRESENT #warning'i tam da
                // bu açığı tarif ediyordu; açık kapandığı için o uyarı kaldırıldı.
                if (!s_stopPendingOpen) {
                    requestZeroTorque();  // (1) — flag 0 iken gerçek frame üretmez
                    s_stopPendingOpen = true;
                    s_zeroTorqueSent = true;
                    s_zeroTorqueAtMs = s_uptimeMs;
                    ESP_LOGI(TAG,
                             "STOP: sifir-tork istendi (%s) — kontaktor acma %u ms sonra",
                             currentState == VcuState::DRIVE ? "DRIVE" : "READY",
                             (unsigned)VCU_CONTACTOR_OPEN_DELAY_MS);
                }
                // Bekleyen bir STOP varken gelen YİNELENEN komut yok sayılır:
                // aksi halde her tekrar bası zamanlayıcıyı sıfırlayıp kontaktör
                // açmayı süresiz erteleyebilirdi.
            } else {
                ESP_LOGW(TAG, "STOP yok sayildi (durum %d — yalniz READY/DRIVE)",
                         static_cast<int>(currentState));
            }
            return;
        }

        // State-specific event handling
        switch (currentState) {
            case VcuState::IDLE:
            {
                if (event == VcuEvent::START_REQUEST) {
                    TelemetryData VCU_snap = getTelemetrySnapshot();
                    bool actuatorFault = s_relays->hasActuatorFault();
                    if (isReadyEntryPermitted(VCU_snap) && !actuatorFault) {
                        if (hasWarningCondition(VCU_snap)) {
                            ESP_LOGW("VCU", "READY girildi ama WARN kosulu aktif!");
                        }
                        transitionTo(VcuState::READY);
                    } else {
                        const char* reason = actuatorFault ? "actuator fault" : readyRejectReason(VCU_snap);
                        if (reason != s_lastReadyRejectReason || s_stateTimer - s_lastReadyRejectLogMs >= 1000) {
                            ESP_LOGW(TAG, "READY gecisi reddedildi: %s", reason);
                            s_lastReadyRejectReason = reason;
                            s_lastReadyRejectLogMs = s_stateTimer;
                        }
                    }
                }
                break;
            }

            case VcuState::READY:
            {
                if (event == VcuEvent::DRIVE_ENABLE) {
                    transitionTo(VcuState::DRIVE);
                }
                break;
            }

            default:
                break;
        }
    }

    // Periodic state logic
    switch (s_state.load(std::memory_order_relaxed)) {
        case VcuState::INIT:
            handleInit();
            break;
        case VcuState::IDLE:
            handleIdle();
            break;
        case VcuState::READY:
            handleReady();
            break;
        case VcuState::DRIVE:
            handleDrive();
            break;
        case VcuState::EMERGENCY_STOP:
            handleEmergencyStop();
            break;
        case VcuState::FAULT:
            handleFault();
            break;
        default:
            break;
    }
}

void postEvent(VcuEvent event) {
    if (event == VcuEvent::EMERGENCY_STOP) {
        // Bypass the queue so a full queue cannot swallow an E-STOP.
        // The flag is checked at the top of run() before any queue drain.
        s_eStopPending.store(true, std::memory_order_release);
        return;
    }
    if (event == VcuEvent::FAULT_DETECTED) {
        // R1: E-STOP ile AYNI desen — kuyruğu BYPASS et, yalnız atomic bayrağı
        // set et. run() en tepede (kuyruk drenajından önce) bunu okur, böylece
        // kuyruk dolu olsa bile FAULT kaybolmaz.
        //
        // Neden kuyruğa da yazMIYORuz: run() tik başına kuyruktan yalnız BİR
        // olay çeker; FAULT'un bir kopyası kuyrukta kalırsa, bayrak yolu zaten
        // FAULT'a aldıktan sonra o bayat kopya bir sonraki tik'i tüketip ardından
        // gelen olayları (ör. RESET) bir tik geciktirir. E-STOP tam da bu yüzden
        // kuyruğu bypass eder; FAULT da aynı kanıtlanmış deseni izler.
        s_faultPending.store(true, std::memory_order_release);
        return;
    }
    if (s_eventQueue == nullptr)
        return;
    if (xQueueSend(s_eventQueue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropped event %d",
                 static_cast<int>(event));
    }
}

VcuState getState() {
    return s_state.load(std::memory_order_relaxed);
}

void setTelemetryData(const TelemetryData& TEL_data) {
    if (s_TEL_dataMutex == nullptr)
        return;

    // Bu mutex bugun tek task tarafindan kullaniliyor; gercek task-arasi veri yolu queue + std::atomic'tir. portMAX_DELAY, watchdog panigi kapaliyken kurtarilamaz kilitlenme kaynagidir. (AKS-21)
    if (xSemaphoreTake(s_TEL_dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGE(TAG, "s_TEL_dataMutex timeout in setTelemetryData");
        return;
    }
    s_TEL_latestData = TEL_data;
    xSemaphoreGive(s_TEL_dataMutex);
}

void setTorqueSink(TorqueSink sink) {
    s_torqueSink = sink;
}

bool isHeadlightOn() {
#if RELAY_ROLES_ASSIGNED
    return s_headlightOn.load(std::memory_order_relaxed);
#else
    // Far mantığı derleme dışı — ekranda dürüstçe "kapalı" gösterilir.
    return false;
#endif
}

#if RELAY_ROLES_ASSIGNED
void setHeadlightSwitchReader(HeadlightSwitchReader reader) {
    s_headlightSwitchReader = reader;
}
#endif

// ---------------------------------------------------------------------------
// State handlers
// ---------------------------------------------------------------------------
static bool s_relaysOpenedInInit = false;
static uint32_t s_lastInitLogMs = 0;

static void handleInit() {
    if (s_stateTimer <= TASK_PERIOD_MS) {
        requestZeroTorque();
    }

    if (s_stateTimer >= VCU_CONTACTOR_OPEN_DELAY_MS) {
        if (!s_relaysOpenedInInit) {
            s_relays->allOff(false);
            s_relaysOpenedInInit = true;
        } else if (s_stateTimer - s_lastInitLogMs >= 1000) {
            s_relays->allOff(true);
        }
    }

    if (s_stateTimer - s_lastInitLogMs >= 1000) {
        ESP_LOGE(TAG, "INIT state — init() failed or incomplete!");
        s_lastInitLogMs = s_stateTimer;
    }
}

static void handleIdle() {
    // All contactors off — safe resting state
    // Waiting for START_REQUEST from LoRa/UKS
#if RELAY_ROLES_ASSIGNED
    // Şartname 8.2.a.iii: şarjda S1 KAPALI + S2 AÇIK. IDLE'da sürüş bankı
    // (S2 dahil) zaten açık; burada yalnız S1, charger freshness'ına
    // (TEL_chargerActive — CAN_chargerValid'den, bayatlama dahil) göre
    // sürülür. Kenar-tetikli: istenen durum değişmedikçe SPI yazılmaz.
    // Charger aktifken START_REQUEST zaten reddedilir (isReadyEntryPermitted).
    const TelemetryData VCU_snap = getTelemetrySnapshot();
    const int8_t desired = VCU_snap.TEL_chargerActive ? 1 : 0;
    if (desired != s_s1LastCmdInIdle) {
        s_relays->setRelay(RELAY_CH_S1_CHARGE, desired == 1);
        ESP_LOGI(TAG, "IDLE: S1 (sarj hatti) %s (chargerActive=%d)",
                 desired == 1 ? "KAPATILDI" : "ACILDI", (int)desired);
        s_s1LastCmdInIdle = desired;
    }
#endif
}

static void handleReady() {
    // Close the drive-side contactors on entry (runs once via stateTimer guard)
    if (s_stateTimer <= TASK_PERIOD_MS) {
#if RELAY_ROLES_ASSIGNED
        // Şartname 8.2.a.vii: sürüşte S1 AÇIK + S2 KAPALI. READY girişi
        // allOn KULLANMAZ (allOn bank maskesini — S1 dahil — kapatırdı);
        // bunun yerine yalnız SÜRÜŞ bankı (RELAY_DRIVE_BANK_MASK = S2 +
        // kanal 1-7; S1 bilinçli olarak bu maskenin DIŞINDA) kapatılır.
        // S1 savunma amaçlı açık komutlanır: IDLE'da charger aktifken READY
        // zaten reddedildiğinden S1 normalde açıktır; bu yazım sırayı
        // şartname durumuna deterministik kilitler.
        s_relays->setRelay(RELAY_CH_S1_CHARGE, false);
        s_relays->setBankStaggered(RELAY_DRIVE_BANK_MASK, RELAY_STAGGER_STEP_MS);
        ESP_LOGI(TAG, "Surus banki kademeli kapatildi (S1 acik) — system READY");
#else
        // Roller atanmadı: eski tek-bank davranışı — bank maskesi (10 kanalın
        // tamamı) kademeli kapatılır.
        s_relays->setBankStaggered(RELAY_CONTACTOR_BANK_MASK, RELAY_STAGGER_STEP_MS);
        ESP_LOGI(TAG, "All contactors closed staggered — system READY");
#endif
    }
    // DRIVE is entered only after an explicit DRIVE_ENABLE command.
    // Future interlocks should be added here before propulsion is allowed.
}

static void handleDrive() {
    // Contactors remain closed during drive.
    // G2: Motor sürücüsü entegre değil (MOTOR_DRIVER_PRESENT=0) — hiçbir torque
    // komutu ÜRETİLMİYOR. Propulsion, motor sürücüsü gelip torque haritalama
    // modeli tanımlanana kadar fiilen devre dışı.
}

static void handleEmergencyStop() {
    // G2 GERÇEĞİ: Motor sürücüsü entegre değil (MOTOR_DRIVER_PRESENT=0). Bu
    // fazda gerçek torque frame'i gönderilmiyor (requestZeroTorque no-op'a
    // düşer); sıra yine de burada KURULU ki entegrasyonda hazır olsun.
    //
    // Güvenli kapanış sırası (flag'ten bağımsız çağrı sırası):
    //   (1) sıfır tork iste  → transitionTo(EMERGENCY_STOP) içinde, t=0'da
    //   (2) VCU_CONTACTOR_OPEN_DELAY_MS bekle
    //   (3) kontaktör aç     → aşağıda, contactorOpenDelayElapsed() kapısıyla
    // (1) burada YAPILMAZ: handler ilk kez t=TASK_PERIOD_MS ile çalıştığından
    // (1) ve (3) aynı tick'e düşerdi — gecikme fiilen sıfır olurdu.

    // (2)+(3): torkun sönmesi için bekle, sonra pozitif kontaktör bankını aç.
    if (contactorOpenDelayElapsed()) {
        if (!s_relaysOpenedInEstop) {
            s_relays->allOff(false); // First time, log it
            s_relaysOpenedInEstop = true;
        } else if (s_stateTimer - s_lastEstopLogMs >= 1000) {
            // G3: sessiz re-assert artık doğrulama DEĞİL sadece log açısından
            // sessiz — allOff() içindeki verifyOutputs() geri-okumayı yapar,
            // uyuşmazsa loglar ve actuator fault'u latch'ler (sürdürür).
            s_relays->allOff(true); // Silent (log) re-assert + verify
        }
    }

    // Log once per second to avoid flooding
    if (s_stateTimer - s_lastEstopLogMs >= 1000) {
        ESP_LOGE(TAG, "EMERGENCY STOP active — all relays off");
        s_lastEstopLogMs = s_stateTimer;
    }
    // Recovery only via physical reset or explicit RESET event
}

static void handleFault() {
    // Güvenli kapanış sırası — handleEmergencyStop ile AYNI desen:
    //   (1) sıfır tork → transitionTo(FAULT) içinde, t=0'da
    //   (2) bekle → (3) kontaktör aç → aşağıda, contactorOpenDelayElapsed() ile.
    // (1)'in neden burada olmadığı için bkz. handleEmergencyStop yorumu.
    if (contactorOpenDelayElapsed()) {
        if (!s_relaysOpenedInFault) {
            s_relays->allOff(false); // First time, log it
            s_relaysOpenedInFault = true;
        } else if (s_stateTimer - s_lastFaultLogMs >= 1000) {
            // G3: allOff() içindeki verifyOutputs() re-assert'i geri-okur;
            // uyuşmazsa loglar + actuator fault latch'lenir (sürdürülür).
            s_relays->allOff(true); // Silent (log) re-assert + verify
        }
    }

    if (s_stateTimer - s_lastFaultLogMs >= 1000) {
        ESP_LOGE(TAG, "FAULT state — send RESET event to recover");
        s_lastFaultLogMs = s_stateTimer;
    }

    if (isResetInterlockSatisfied()) {
        s_autoResetTimer += TASK_PERIOD_MS;
        if (s_autoResetTimer >= VCU_AUTO_RESET_DELAY_MS) {
            ESP_LOGW(TAG, "Otomatik FAULT reset (ekran kopuklugu telafisi)");
            postEvent(VcuEvent::RESET);
            s_autoResetTimer = 0;
        }
    } else {
        s_autoResetTimer = 0;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
// P3 güvenli kapanış sırasının 1. adımı: sıfır tork iste. Sink bağlıysa
// (üretimde CanManager::sendTorqueCommand) çağrılır; MOTOR_DRIVER_PRESENT=0
// iken orada gerçek frame ÜRETİLMEZ. Sink bağlı değilse istek yok sayılır.
static void requestZeroTorque() {
    if (s_torqueSink != nullptr)
        s_torqueSink(0);
}

// P3 sırasının 1. ADIMI, t=0'da. Sıfır torku ister ve zaman damgasını kurar;
// kontaktör açma bundan sonra contactorOpenDelayElapsed()'e bağlıdır.
// MOTOR_DRIVER_PRESENT=0 iken requestZeroTorque bir NO-OP'tur (sink gerçek
// frame üretmez) — davranış değişmez, DEĞİŞEN yalnızca SIRA GARANTİSİDİR.
static void beginSafeShutdown() {
    requestZeroTorque();
    s_zeroTorqueSent = true;
    s_zeroTorqueAtMs = s_uptimeMs;
}

// P3 sırasının 3. ADIMININ kapısı: sıfır tork istendi VE üzerinden en az
// VCU_CONTACTOR_OPEN_DELAY_MS geçti mi? Yukarıdaki static_assert bu gecikmenin
// >= bir tik olmasını garanti ettiğinden, true dönmesi kontaktör açmanın
// sıfır-tork tick'inden FARKLI (ve sonraki) bir tick'te olduğu anlamına gelir.
// İşaretsiz çıkarma idiomu s_uptimeMs sarmasında da doğru çalışır.
static bool contactorOpenDelayElapsed() {
    return s_zeroTorqueSent &&
           (s_uptimeMs - s_zeroTorqueAtMs) >= VCU_CONTACTOR_OPEN_DELAY_MS;
}

static void transitionTo(VcuState next) {
    VcuState current = s_state.load(std::memory_order_relaxed);
    ESP_LOGI(TAG, "State: %d → %d", static_cast<int>(current),
             static_cast<int>(next));
    s_state.store(next, std::memory_order_relaxed);
    s_stateTimer = 0;

    // Bekleyen bir kontrollü durdurma (STOP) varsa İPTAL: durum başka bir
    // nedenle değişti (E-STOP/FAULT kendi kapanış sırasını yürütür) — bayat bir
    // STOP tamamlanıp güvenlik durumunu IDLE'a DÜŞÜRMEMELİDİR.
    s_stopPendingOpen = false;

    if (next == VcuState::EMERGENCY_STOP) {
        s_relaysOpenedInEstop = false;
        s_lastEstopLogMs = (uint32_t)-1000;
        // P3 adım (1): sıfır tork t=0'da, GEÇİŞ TICK'İNDE istenir. Handler'ın
        // ilk tick'ine bırakılamaz — o tick'te kontaktör açma koşulu da
        // sağlanıyor ve ikisi aynı tick'e düşüyordu (bkz. dosya başındaki not).
        beginSafeShutdown();
        ESP_LOGW(TAG, "EMERGENCY STOP: sifir-tork istegi (motor surucusu yoksa atlanir)");
    } else if (next == VcuState::FAULT) {
        s_relaysOpenedInFault = false;
        s_lastFaultLogMs = (uint32_t)-1000;
        s_autoResetTimer = 0;
        beginSafeShutdown();  // P3 adım (1) — E-STOP ile aynı gerekçe
        ESP_LOGW(TAG, "FAULT: sifir-tork istegi (motor surucusu yoksa atlanir)");
    } else {
        // Güvenli kapanış epizodu bitti (IDLE/READY/DRIVE'a dönüldü).
        s_zeroTorqueSent = false;
    }

#if RELAY_ROLES_ASSIGNED
    if (next == VcuState::IDLE) {
        // IDLE'a her girişte S1 komut izini "bilinmiyor" yap: FAULT/E-STOP
        // yolunda allOff S1'i açmış olabilir — ilk IDLE tick'i S1'i charger
        // durumuna göre deterministik olarak yeniden yazar (bkz. handleIdle).
        s_s1LastCmdInIdle = -1;
    }
#endif
}

static bool pollEvent(VcuEvent& out) {
    if (s_eventQueue == nullptr)
        return false;
    return xQueueReceive(s_eventQueue, &out, 0) == pdTRUE;
}

static TelemetryData getTelemetrySnapshot() {
    if (s_TEL_dataMutex == nullptr)
        return s_TEL_latestData;

    TelemetryData VCU_dataCopy = {};
    // Bu mutex bugun tek task tarafindan kullaniliyor; gercek task-arasi veri yolu queue + std::atomic'tir. portMAX_DELAY, watchdog panigi kapaliyken kurtarilamaz kilitlenme kaynagidir. (AKS-21)
    if (xSemaphoreTake(s_TEL_dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGE(TAG, "s_TEL_dataMutex timeout in getTelemetrySnapshot");
        return VCU_dataCopy;
    }
    VCU_dataCopy = s_TEL_latestData; // <-- V HARFİ DÜZELTİLDİ
    xSemaphoreGive(s_TEL_dataMutex);
    return VCU_dataCopy;
}

static bool isResetInterlockSatisfied() {
    return isResetInterlockSatisfied(getTelemetrySnapshot(), s_state.load(std::memory_order_relaxed));
}

static bool hasWarningCondition() {
    return hasWarningCondition(getTelemetrySnapshot());
}

static bool hasCriticalCondition() {
    return hasCriticalCondition(getTelemetrySnapshot(), s_state.load(std::memory_order_relaxed));
}

// isReadyEntryPermitted() reddettiğinde hangi koşulun sağlanmadığını döndürür.
// Sıralama predicate ile birebir aynı olmalı; loglama için statik literal
// döndürür (pointer karşılaştırmasıyla "neden değişti mi" tespiti için).
static const char* readyRejectReason(const TelemetryData& VCU_data) {
    if (!VCU_data.TEL_bmsDataValid)
        return "bmsDataValid=0";
#if RELAY_ROLES_ASSIGNED
    // Şartname 8.2.a.iii — sıralama isReadyEntryPermitted ile birebir aynı.
    if (VCU_data.TEL_chargerActive)
        return "charger aktif — sarj modunda READY yasak";
#endif
    if (hasCriticalCondition(VCU_data, VcuState::IDLE))
        return "kritik kosul aktif";
    // AKS-14: Uyari kosullari READY girisini bloklamaz.
    // if (hasWarningCondition(VCU_data))
    //     return "uyari kosulu aktif";
#if MOTOR_DRIVER_PRESENT
    if (!VCU_data.TEL_motorDataValid)
        return "motorDataValid=0";
#endif
    return "bilinmiyor";
}

// Motor timeout detection already lives in CanParse::isMotorStatusTimedOut +
// CanManager::updateMotorStatusValidity; if the Teknofest spec needs an
// error-flag bit for this, it should hook into that timeout path, not a
// separate one here (separate task).

#ifdef NATIVE_BUILD
void resetForTest() {
    s_state.store(VcuState::INIT, std::memory_order_relaxed);
    s_stateTimer = 0;
    s_uptimeMs = 0;
    s_lastTimeMs = 0;
    s_TEL_latestData = {};
    s_VCU_warningLogged = false;
    s_eStopPending.store(false, std::memory_order_relaxed);
    s_faultPending.store(false, std::memory_order_relaxed);
    s_relays = nullptr;  // init() yeniden bağlar

    s_relaysOpenedInInit = false;
    s_relaysOpenedInEstop = false;
    s_relaysOpenedInFault = false;
    s_zeroTorqueSent = false;
    s_zeroTorqueAtMs = 0;
    s_stopPendingOpen = false;
    s_lastInitLogMs = 0;
    s_lastEstopLogMs = 0;
    s_lastFaultLogMs = 0;
    s_autoResetTimer = 0;
    s_lastReadyRejectReason = nullptr;
    s_lastReadyRejectLogMs = 0;
    s_torqueSink = nullptr;
#if RELAY_ROLES_ASSIGNED
    s_flasherOn = false;
    s_s1LastCmdInIdle = -1;
    s_fanOn = false;
    s_headlightOn.store(false, std::memory_order_relaxed);
    s_headlightSwitch = HeadlightSwitch::State{};
    s_headlightSwitchReader = nullptr;
#endif

    // Olay kuyruğunu (queue) boşalt
    if (s_eventQueue != nullptr) {
        VcuEvent drained = VcuEvent::NONE;
        while (xQueueReceive(s_eventQueue, &drained, 0) == pdTRUE) {
            // discard
        }
    }
}
#endif

}  // namespace VcuLogic
