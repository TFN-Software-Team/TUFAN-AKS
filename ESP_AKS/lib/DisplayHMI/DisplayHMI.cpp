#include "DisplayHMI.h"
#include "SystemConfig.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>

static constexpr const char *TAG = "DisplayHMI";

DisplayHMI::DisplayHMI()
    : HMI_isInitialized(false),
      HMI_hasCachedScreen(false),
      HMI_resetPending(false),
      HMI_resetWarnLoggedOnce(false),
      HMI_lastResetWarnTick(0),
      HMI_resetCount(0),
      m_lastRxTimeMs(0),
      m_aliveWarnLoggedOnce(false),
      m_lastAliveWarnTick(0),
      m_lastSendmeTick(0),
      HMI_lastResyncTick(0),
      HMI_nextResyncField(0),
      HMI_lastPackvSendTick(0),
      HMI_lastPackaSendTick(0),
      HMI_lastScreenData({}) {}

bool DisplayHMI::begin() {
    if (HMI_isInitialized) return true;

    uart_config_t HMI_uartConfig = {
        .baud_rate = HMI_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT
    };

    if (uart_param_config(HMI_UART_NUM, &HMI_uartConfig) != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed");
        return false;
    }

    if (uart_set_pin(HMI_UART_NUM, HMI_TX_PIN, HMI_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed (TX=%d, RX=%d)", HMI_TX_PIN, HMI_RX_PIN);
        return false;
    }

    if (uart_driver_install(HMI_UART_NUM, 1024, 1024, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed");
        return false;
    }

    HMI_isInitialized = true;

    // Nextion açılış mesajlarını temizle
    HMI_drainRxBuffer();

    // Nextion acknowledge yanıtlarını kapat (bkcmd=0)
    // Aksi halde her komut sonrası gelen 0x01/0x02/0x03 yanıtları
    // readTouchCommand tarafından sahte komut olarak yorumlanır
    HMI_sendBkcmd0();
    vTaskDelay(pdMS_TO_TICKS(50));  // Nextion'ın işlemesi için bekle
    HMI_drainRxBuffer();            // bkcmd komutunun kendi acknowledge'ını temizle

    ESP_LOGI(TAG, "Initialized on UART%d (TX=IO%d, RX=IO%d)", HMI_UART_NUM, HMI_TX_PIN, HMI_RX_PIN);
    return true;
}

void DisplayHMI::HMI_drainRxBuffer() {
    uint8_t HMI_drainBuf[32];
    while (uart_read_bytes(HMI_UART_NUM, HMI_drainBuf, sizeof(HMI_drainBuf), 0) > 0) {
        // Nextion acknowledge/error yanıtlarını temizle
    }
}

// bkcmd=0 Nextion'da KALICI DEĞİLDİR — ekran reset'inde Editor varsayılanına
// döner; hem begin()'de hem reset kurtarmasında gönderilir.
void DisplayHMI::HMI_sendBkcmd0() {
    const char *HMI_bkcmd = "bkcmd=0";
    uart_write_bytes(HMI_UART_NUM, HMI_bkcmd, 7);
    HMI_sendEndBytes();
}

void DisplayHMI::forceFullRefresh() { HMI_hasCachedScreen = false; }

bool DisplayHMI::consumeResetFlag() {
    const bool HMI_was = HMI_resetPending;
    HMI_resetPending = false;
    return HMI_was;
}

bool DisplayHMI::isDisplayAlive() {
    uint32_t nowMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool alive = (nowMs - m_lastRxTimeMs) < HMI_LINK_TIMEOUT_MS;
    
    if (!alive) {
        if (!m_aliveWarnLoggedOnce || (nowMs - m_lastAliveWarnTick) >= 5000) {
            ESP_LOGE(TAG, "Nextion ekran ile baglanti koptu! (Son yanit: %lu ms once)", (unsigned long)(nowMs - m_lastRxTimeMs));
            m_lastAliveWarnTick = nowMs;
            m_aliveWarnLoggedOnce = true;
        }
    } else {
        m_aliveWarnLoggedOnce = false;
    }
    return alive;
}

// Startup event (00 00 00 FF FF FF) yakalandı: Nextion brown-out/reset attı,
// tüm component'ler Editor varsayılanına döndü. Kurtarma:
//   (a) bkcmd=0'ı yeniden gönder (reset ile kaybolur),
//   (b) DisplayHMI cache'ini geçersiz kıl → bir sonraki updateScreen tüm
//       skalar alanları yeniden basar (boot'taki ilk çağrıyla birebir aynı yol),
//   (c) HMI_resetPending → HMI_Task consumeResetFlag() ile BmsNextionCache'i
//       sıfırlar; hücre barları maxBytes bütçesiyle döngülere yayılarak dolar.
// Burada drain/delay YAPILMAZ — readTouchCommand'ın byte akışı bozulmamalı.
void DisplayHMI::HMI_handleNextionReset() {
    ++HMI_resetCount;
    HMI_sendBkcmd0();
    forceFullRefresh();
    HMI_resetPending = true;

    // Oran-sınırlı WARN (CAN_RX_STATS_LOG_INTERVAL_MS deseni): ekran güç
    // hattında sürekli brown-out varsa log spam yapılmaz, toplam sayaçla
    // en fazla 1 WARN / HMI_RESET_WARN_LOG_INTERVAL_MS basılır.
    const uint32_t HMI_now = xTaskGetTickCount();
    if (!HMI_resetWarnLoggedOnce ||
        (HMI_now - HMI_lastResetWarnTick) >=
            pdMS_TO_TICKS(HMI_RESET_WARN_LOG_INTERVAL_MS)) {
        ESP_LOGW(TAG,
                 "Nextion reset algilandi (toplam %lu), ekran yeniden "
                 "dolduruluyor",
                 (unsigned long)HMI_resetCount);
        HMI_lastResetWarnTick = HMI_now;
        HMI_resetWarnLoggedOnce = true;
    }
}

void DisplayHMI::updateScreen(const HMI_DisplayData& HMI_data) {
    if (!HMI_isInitialized) return;

    uint32_t nowMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (nowMs - m_lastSendmeTick >= 1000) {
        m_lastSendmeTick = nowMs;
        const char cmd[9] = {'s','e','n','d','m','e', (char)0xFF, (char)0xFF, (char)0xFF};
        uart_write_bytes(HMI_UART_NUM, cmd, 9);
    }

    const bool HMI_forceRefresh = !HMI_hasCachedScreen;

    // packv/packa tavanı atlarsa geri yüklenecek cache değerleri (aşağıya bkz.).
    const uint16_t HMI_packvCachedBeforeUpdate =
        HMI_lastScreenData.HMI_bmsPackVoltageDeciV;
    const int32_t HMI_packaCachedBeforeUpdate =
        HMI_lastScreenData.HMI_bmsPackCurrentCentiA;

    // Round-robin resync emniyet katmanı (bkz. ResyncPolicy.h): Startup
    // event'i brown-out sırasında RX hattında kaybolursa reset dedektörü kör
    // kalır — bu yüzden her HMI_RESYNC_INTERVAL_MS'te bir SIRADAKİ TEK alan
    // cache'e bakılmaksızın zorla gönderilir (burst yok, bütçe aşımı yok).
    const int HMI_resyncField = hmi_resync_due_field(
        xTaskGetTickCount(), HMI_lastResyncTick, HMI_nextResyncField,
        HMI_RESYNC_FIELD_COUNT, pdMS_TO_TICKS(HMI_RESYNC_INTERVAL_MS));
    const auto HMI_force = [&](HMI_ResyncField HMI_field) {
        return HMI_forceRefresh || HMI_resyncField == (int)HMI_field;
    };

    // DEVRE DIŞI (MOTOR_DRIVER_PRESENT=0): 0x200 frame'ini bugün hall-effect hız
    // sensörü üretiyor ve data[7]=0x00 gönderiyor — bu alan yapısal olarak hep
    // "0x00" basıyordu. Üstelik ham hex, bit anlamları belgelenmemiş
    // (CAN_Message_Table.md byte 7: bit0=çalışıyor, bit[7:1]=hata, tek tek anlam
    // BİLİNMİYOR) ve "hata yok" ile "veri yok" ayrımı bu alanda YOK — o ayrım
    // `valid` alanında (VALID/INVALID/TIMEOUT).
    // GERİ AÇMADAN ÖNCE: (1) sürücü dokümanından bit haritasını çıkar ve
    // CAN_Message_Table.md'ye işle, (2) HMI_formatErrorText'i insan-okur metne
    // çevir, (3) veri yokken "--" sentinel'i bas, (4) txt_maxl ve
    // HMI_RESYNC_CMD_MAX_BYTES bütçesini yeni metin uzunluğuna göre kontrol et.
    // char HMI_currentErrorText[16];
    // char HMI_lastErrorText[16];
    //
    // HMI_formatErrorText(HMI_data.HMI_motorErrorFlags, HMI_currentErrorText,
    //                     sizeof(HMI_currentErrorText));
    // HMI_formatErrorText(HMI_lastScreenData.HMI_motorErrorFlags,
    //                     HMI_lastErrorText, sizeof(HMI_lastErrorText));

    HMI_sendNumericIfChanged("speed", HMI_data.HMI_currentSpeed,
                             HMI_lastScreenData.HMI_currentSpeed,
                             HMI_force(HMI_RESYNC_SPEED));
    HMI_sendNumericIfChanged("bat", HMI_data.HMI_currentBattery,
                             HMI_lastScreenData.HMI_currentBattery,
                             HMI_force(HMI_RESYNC_BAT));
    HMI_sendNumericIfChanged("rpm", HMI_data.HMI_motorRpm,
                             HMI_lastScreenData.HMI_motorRpm,
                             HMI_force(HMI_RESYNC_RPM));
    HMI_sendNumericIfChanged("torque", HMI_data.HMI_motorTorqueFeedback,
                             HMI_lastScreenData.HMI_motorTorqueFeedback,
                             HMI_force(HMI_RESYNC_TORQUE));
    HMI_sendNumericIfChanged("temp", HMI_data.HMI_bmsTemperatureC,
                             HMI_lastScreenData.HMI_bmsTemperatureC,
                             HMI_force(HMI_RESYNC_TEMP));
    // packv Nextion'da 1 ondalıklı, packa 2 ondalıklı float (xfloat) —
    // ".val" packv için gerçek_değer×10, packa için gerçek_değer×100
    // olacak şekilde ölçeklenir (bkz. HMIHelpers.h).
    //
    // packv ve packa alanlarında gösterim tavanı vardır (UpdateThrottle.h):
    // akım/gerilim sürekli oynadığından change-compare ile ekranda okunamayacak
    // kadar hızlı zıplamayı ve seri alım kuyruğu birikmesini önler.
    // Force yolları (tam yenileme + round-robin resync) tavanı BAĞLAMAZ.
    const bool HMI_packvForce = HMI_force(HMI_RESYNC_PACKV);
    const bool HMI_packvSend =
        HMI_packvForce ||
        hmi_throttle_due(xTaskGetTickCount(), HMI_lastPackvSendTick,
                         pdMS_TO_TICKS(HMI_PACKV_MIN_UPDATE_INTERVAL_MS));
    if (HMI_packvSend) {
        HMI_sendNumericIfChanged(
            "packv", HMI_packVoltageToXfloat(HMI_data.HMI_bmsPackVoltageDeciV),
            HMI_packVoltageToXfloat(HMI_lastScreenData.HMI_bmsPackVoltageDeciV),
            HMI_packvForce);
        hmi_throttle_stamp(xTaskGetTickCount(), HMI_lastPackvSendTick);
    }

    const bool HMI_packaForce = HMI_force(HMI_RESYNC_PACKA);
    const bool HMI_packaSend =
        HMI_packaForce ||
        hmi_throttle_due(xTaskGetTickCount(), HMI_lastPackaSendTick,
                         pdMS_TO_TICKS(HMI_PACKA_MIN_UPDATE_INTERVAL_MS));
    if (HMI_packaSend) {
        HMI_sendNumericIfChanged(
            "packa", HMI_packCurrentToXfloat(HMI_data.HMI_bmsPackCurrentCentiA),
            HMI_packCurrentToXfloat(HMI_lastScreenData.HMI_bmsPackCurrentCentiA),
            HMI_packaForce);
        hmi_throttle_stamp(xTaskGetTickCount(), HMI_lastPackaSendTick);
    }

    HMI_sendTextIfChanged("state", HMI_getStateText(HMI_data.HMI_vcuState),
                          HMI_getStateText(HMI_lastScreenData.HMI_vcuState),
                          HMI_force(HMI_RESYNC_STATE));
    // DEVRE DIŞI — yukarıdaki gerekçeye bakınız. HMI_RESYNC_MOTOR_ERR enum
    // girdisi ATIL SLOT olarak yerinde bırakıldı (sıra kaydırmamak için).
    // HMI_sendTextIfChanged("motorErr", HMI_currentErrorText, HMI_lastErrorText,
    //                       HMI_force(HMI_RESYNC_MOTOR_ERR));
    HMI_sendTextIfChanged("valid",
                          HMI_getValidityText(HMI_data.HMI_motorDataValid,
                                              HMI_data.HMI_motorTimeoutActive),
                          HMI_getValidityText(
                              HMI_lastScreenData.HMI_motorDataValid,
                              HMI_lastScreenData.HMI_motorTimeoutActive),
                          HMI_force(HMI_RESYNC_VALID));
    HMI_sendTextIfChanged("contactor",
                          HMI_getContactorText(HMI_data.HMI_contactorClosed),
                          HMI_getContactorText(
                              HMI_lastScreenData.HMI_contactorClosed),
                          HMI_force(HMI_RESYNC_CONTACTOR));

    // Şarj/deşarj durumu göstergesi: "chg.val=<0..3>" — ekrandaki tm0 timer'ı
    // bunu chgtxt metnine çevirir (0=Bosta, 1=Sarj Oluyor, 2=Desarj, 3="--").
    // Karar SAF hmi_chargeState'te (lib/HMIHelpers/ChargeState.h); burada
    // yalnız mevcut ve önceki snapshot için ayrı ayrı hesaplanıp
    // change-compare'a verilir (packv/packa deseniyle birebir aynı).
    HMI_sendNumericIfChanged(
        "chg",
        hmi_chargeState(HMI_data.HMI_bmsDataValid,
                        HMI_data.HMI_bmsTimeoutActive,
                        HMI_data.HMI_chargerActive,
                        HMI_data.HMI_bmsPackCurrentCentiA,
                        HMI_CHG_DISCHARGE_DEADBAND_CENTI_A),
        hmi_chargeState(HMI_lastScreenData.HMI_bmsDataValid,
                        HMI_lastScreenData.HMI_bmsTimeoutActive,
                        HMI_lastScreenData.HMI_chargerActive,
                        HMI_lastScreenData.HMI_bmsPackCurrentCentiA,
                        HMI_CHG_DISCHARGE_DEADBAND_CENTI_A),
        HMI_force(HMI_RESYNC_CHG));

    // Far durumu göstergesi (şartname B2 9.19.c): "far.pic=<ID>" — bool durum
    // Nextion resource ID'sine eşlenir (HMI_PIC_HEADLIGHT_ON/OFF, SystemConfig.h).
    // change-compare + resync deseni diğer alanlarla birebir aynı; enum sırası
    // (HMI_RESYNC_HEADLIGHT, ResyncPolicy.h) bu gönderimle SON sırada eşleşir.
    HMI_sendPicIfChanged(
        "far",
        HMI_data.HMI_headlightOn ? HMI_PIC_HEADLIGHT_ON : HMI_PIC_HEADLIGHT_OFF,
        HMI_lastScreenData.HMI_headlightOn ? HMI_PIC_HEADLIGHT_ON
                                           : HMI_PIC_HEADLIGHT_OFF,
        HMI_force(HMI_RESYNC_HEADLIGHT));

    HMI_lastScreenData = HMI_data;
    // packv/packa tavan yüzünden atlandıysa cache'i ESKİ değerde bırak: aksi halde
    // change-compare bir sonraki tikte "değişmedi" der ve o güncelleme SONSUZA
    // dek kaybolurdu.
    // Böylece bekleyen değişiklik yapışkan kalır ve pencere açılınca EN SON
    // değer yazılır.
    if (!HMI_packvSend) {
        HMI_lastScreenData.HMI_bmsPackVoltageDeciV =
            HMI_packvCachedBeforeUpdate;
    }
    if (!HMI_packaSend) {
        HMI_lastScreenData.HMI_bmsPackCurrentCentiA =
            HMI_packaCachedBeforeUpdate;
    }
    HMI_hasCachedScreen = true;
}

bool DisplayHMI::readTouchCommand(uint8_t& HMI_command) {
    if (!HMI_isInitialized) return false;

// --- HMI Command Frame Format ---
    // The HMI must send commands as a 3-byte frame to ensure integrity:
    // [HEADER] [COMMAND] [CHECKSUM]
    // HEADER   = 0x5A
    // COMMAND  = HMI_CMD_... (e.g. 0x01 for START)
    // CHECKSUM = ~COMMAND (bitwise NOT of COMMAND)
    // 
    // Example START frame: 0x5A 0x01 0xFE
    
    uint8_t rxByte;
    // Timeout pdMS_TO_TICKS(10) ile en az 1 byte bekler (eski davranis),
    // ardindan bufferdaki kalan bytelari 0 timeout ile ceker.
    int rxBytes = uart_read_bytes(HMI_UART_NUM, &rxByte, 1, pdMS_TO_TICKS(10));
    if (rxBytes <= 0) return false;

    do {
        if (rxByte == 0x65 || rxByte == 0x66 || rxByte == 0x5A) {
            m_lastRxTimeMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
        }

        // Nextion reset dedektörü ham byte akışını PARALEL gözler — aşağıdaki
        // 0x5A çerçeve mantığından tamamen bağımsızdır (Startup event'inin
        // 0x00/0xFF byte'ları zaten çerçeve parser'ında atlanıyor).
        if (HMI_resetDetect.HMI_feedByte(rxByte)) {
            HMI_handleNextionReset();
        }

        if (HMI_parseTouchByte(rxByte, HMI_touchParserState, HMI_command)) {
            return true;
        }
    } while (uart_read_bytes(HMI_UART_NUM, &rxByte, 1, 0) == 1);

    return false;
}
