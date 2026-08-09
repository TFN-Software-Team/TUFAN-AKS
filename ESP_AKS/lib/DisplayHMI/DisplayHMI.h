#pragma once
#include <cstddef>
#include <cstdint>

#include "ChargeState.h"
#include "HMIHelpers.h"
#include "HMITouchParser.h"
#include "NextionResetDetect.h"
#include "ResyncPolicy.h"
#include "UpdateThrottle.h"

// HMI_VcuState ve saf metin/komut helper'ları HMIHelpers.h'ye taşındı; bu
// header geriye uyumluluk için onu yeniden expose eder. DisplayHMI sınıfı
// yalnızca init / cache / RX state'ini yönetir.
struct HMI_DisplayData {
    uint16_t HMI_currentSpeed;
    uint8_t HMI_currentBattery;
    uint16_t HMI_motorRpm;
    int16_t HMI_motorTorqueFeedback;
    uint8_t HMI_motorErrorFlags;
    bool HMI_motorDataValid;
    bool HMI_motorTimeoutActive;
    int16_t HMI_bmsTemperatureC;
    uint16_t HMI_bmsPackVoltageDeciV;
    int32_t HMI_bmsPackCurrentCentiA;
    // BMS tazeliği — `chg` (şarj/deşarj göstergesi) kararının NO_DATA dalı için
    // ŞART (hmi_chargeState, lib/HMIHelpers/ChargeState.h). Motor tarafındaki
    // HMI_motorDataValid/HMI_motorTimeoutActive bunun YERİNE KULLANILAMAZ:
    // 0x200 frame'ini bugün hall-effect hız sensörü üretiyor, BMS E000/E001
    // akışından tamamen bağımsız. Motor bayrağına bakılsaydı BMS sustuğu halde
    // ekran bayat akımdan "Bosta"/"Desarj" türetirdi.
    bool HMI_bmsDataValid;
    bool HMI_bmsTimeoutActive;
    // Araç şarjda mı (TEL_chargerActive). Ekran bunu yalnız GÖSTERİR; şarj
    // tespitinin kendisi CanManager'da yapılır (charger frame tazeliği VEYA
    // ChargeDetect akım-işareti tespiti, OR'lu) — burada YENİDEN ÜRETİLMEZ.
    bool HMI_chargerActive;
    bool HMI_contactorClosed;
    HMI_VcuState HMI_vcuState;
    // Far durumu (şartname B2 9.19.c) — ekran yalnız GÖSTERİR (far.pic), farı
    // KONTROL ETMEZ. Durumun tek sahibi ESP (VcuLogic::isHeadlightOn).
    bool HMI_headlightOn;
};

class DisplayHMI {
   private:
    bool HMI_isInitialized;
    bool HMI_hasCachedScreen;
    // Reset dedektörü tetiklendi, dış tüketici (HMI_Task → BmsNextionCache)
    // henüz consumeResetFlag() ile okumadı.
    bool HMI_resetPending;
    bool HMI_resetWarnLoggedOnce;
    uint32_t HMI_lastResetWarnTick;
    uint32_t HMI_resetCount;
    uint32_t m_lastRxTimeMs;
    bool m_aliveWarnLoggedOnce;
    uint32_t m_lastAliveWarnTick;
    // Round-robin resync durumu (bkz. ResyncPolicy.h — Startup event'i RX
    // hattında kaybolursa devreye giren periyodik emniyet katmanı).
    uint32_t HMI_lastResyncTick;
    uint8_t HMI_nextResyncField;
    // packv gösterim tavanı (bkz. UpdateThrottle.h): en son GERÇEKTEN packv
    // gönderilen tick. Tavan aşılmadıysa alan atlanır ve HMI_lastScreenData'daki
    // packv girdisi ESKİ değerde bırakılır — bekleyen değişiklik kaybolmaz.
    uint32_t HMI_lastPackvSendTick;
    HMI_DisplayData HMI_lastScreenData;
    HMI_NextionResetDetect HMI_resetDetect;
    HMI_TouchParserState HMI_touchParserState;

    void HMI_drainRxBuffer();
    void HMI_sendBkcmd0();
    void HMI_handleNextionReset();

   public:
    DisplayHMI();
    bool begin();
    void updateScreen(const HMI_DisplayData& HMI_data);
    bool readTouchCommand(uint8_t& HMI_command);
    // Bir sonraki updateScreen çağrısının TÜM alanları (değişmemiş olsa da)
    // yeniden göndermesini sağlar. Nextion reset kurtarmasında dedektör
    // tarafından çağrılır; dışarıya da açıktır (ör. sayfa değişimi).
    void forceFullRefresh();
    // Nextion reset algılandıysa true döner ve bayrağı temizler (tek-atımlık).
    // Tüketici SADECE BmsNextionCache/BMS_firstRun tarafıdır — DisplayHMI'nin
    // kendi cache'i forceFullRefresh() ile zaten dahili olarak sıfırlanır.
    bool consumeResetFlag();

    bool isDisplayAlive();
};
