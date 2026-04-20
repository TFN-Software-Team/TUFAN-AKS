#include "DisplayHMI.h"
#include "SystemConfig.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>

static constexpr const char *TAG = "DisplayHMI";

DisplayHMI::DisplayHMI() : HMI_isInitialized(false) {}

bool DisplayHMI::begin() {
    if (HMI_isInitialized) return true;

    uart_config_t HMI_uartConfig = {
        .baud_rate = 9600,
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

    if (uart_driver_install(HMI_UART_NUM, 256, 256, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed");
        return false;
    }

    HMI_isInitialized = true;

    // Nextion açılış mesajlarını temizle
    HMI_drainRxBuffer();

    // Nextion acknowledge yanıtlarını kapat (bkcmd=0)
    // Aksi halde her komut sonrası gelen 0x01/0x02/0x03 yanıtları
    // readTouchCommand tarafından sahte komut olarak yorumlanır
    const char *HMI_bkcmd = "bkcmd=0";
    uart_write_bytes(HMI_UART_NUM, HMI_bkcmd, 7);
    HMI_sendEndBytes();
    vTaskDelay(pdMS_TO_TICKS(50));  // Nextion'ın işlemesi için bekle
    HMI_drainRxBuffer();            // bkcmd komutunun kendi acknowledge'ını temizle

    ESP_LOGI(TAG, "Initialized on UART%d (TX=IO%d, RX=IO%d)", HMI_UART_NUM, HMI_TX_PIN, HMI_RX_PIN);
    return true;
}

void DisplayHMI::HMI_sendEndBytes() {
    const uint8_t HMI_endBytes[3] = {0xFF, 0xFF, 0xFF};
    uart_write_bytes(HMI_UART_NUM, (const char*)HMI_endBytes, 3);
}

void DisplayHMI::HMI_drainRxBuffer() {
    uint8_t HMI_drainBuf[32];
    while (uart_read_bytes(HMI_UART_NUM, HMI_drainBuf, sizeof(HMI_drainBuf), 0) > 0) {
        // Nextion acknowledge/error yanıtlarını temizle
    }
}

void DisplayHMI::updateScreen(uint16_t HMI_speed, uint8_t HMI_battery) {
    if (!HMI_isInitialized) return;

    char HMI_speedCmd[32];
    int HMI_speedLen = snprintf(HMI_speedCmd, sizeof(HMI_speedCmd), "speed.val=%u", HMI_speed);
    uart_write_bytes(HMI_UART_NUM, HMI_speedCmd, HMI_speedLen);
    HMI_sendEndBytes();

    char HMI_batCmd[32];
    int HMI_batLen = snprintf(HMI_batCmd, sizeof(HMI_batCmd), "bat.val=%u", HMI_battery);
    uart_write_bytes(HMI_UART_NUM, HMI_batCmd, HMI_batLen);
    HMI_sendEndBytes();
}

bool DisplayHMI::readTouchCommand(uint8_t& HMI_command) {
    if (!HMI_isInitialized) return false;

    uint8_t HMI_rxBuf[1];
    int HMI_rxBytes = uart_read_bytes(HMI_UART_NUM, HMI_rxBuf, 1, pdMS_TO_TICKS(10));

    if (HMI_rxBytes <= 0) return false;

    // Gürültü byte'larını filtrele:
    // 0xFF = Nextion end-byte (buffer'da kalan artık)
    // 0x00 = Nextion Invalid Instruction yanıtı
    // bkcmd=0 sayesinde 0x01-0x05 ack yanıtları gelmez,
    // dolayısıyla bu değerler güvenle komut olarak yorumlanabilir
    if (HMI_rxBuf[0] == 0xFF || HMI_rxBuf[0] == 0x00) return false;

    HMI_command = HMI_rxBuf[0];
    return true;
}
