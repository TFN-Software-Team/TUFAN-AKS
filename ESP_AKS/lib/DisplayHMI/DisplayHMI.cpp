#include "DisplayHMI.h"
#include "SystemConfig.h"
#include "driver/uart.h"
#include <cstdio>

DisplayHMI::DisplayHMI() : HMI_isInitialized(false) {}

bool DisplayHMI::begin() {
    uart_config_t HMI_uartConfig = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT
    };

    uart_param_config(HMI_UART_NUM, &HMI_uartConfig);
    uart_set_pin(HMI_UART_NUM, HMI_TX_PIN, HMI_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    if (uart_driver_install(HMI_UART_NUM, 256, 256, 0, nullptr, 0) == ESP_OK) {
        HMI_isInitialized = true;
        return true;
    }
    return false;
}

void DisplayHMI::HMI_sendEndBytes() {
    const uint8_t HMI_endBytes[3] = {0xFF, 0xFF, 0xFF};
    uart_write_bytes(HMI_UART_NUM, (const char*)HMI_endBytes, 3);
}

void DisplayHMI::updateScreen(uint8_t HMI_speed, uint8_t HMI_battery) {
    if (!HMI_isInitialized) return;

    char HMI_speedCmd[32];
    int HMI_speedLen = snprintf(HMI_speedCmd, sizeof(HMI_speedCmd), "speed.val=%d", HMI_speed);
    uart_write_bytes(HMI_UART_NUM, HMI_speedCmd, HMI_speedLen);
    HMI_sendEndBytes();

    char HMI_batCmd[32];
    int HMI_batLen = snprintf(HMI_batCmd, sizeof(HMI_batCmd), "bat.val=%d", HMI_battery);
    uart_write_bytes(HMI_UART_NUM, HMI_batCmd, HMI_batLen);
    HMI_sendEndBytes();
}

bool DisplayHMI::readTouchCommand(uint8_t& HMI_command) {
    if (!HMI_isInitialized) return false;

    uint8_t HMI_rxBuf[1];
    int HMI_rxBytes = uart_read_bytes(HMI_UART_NUM, HMI_rxBuf, 1, pdMS_TO_TICKS(10));

    if (HMI_rxBytes > 0) {
        HMI_command = HMI_rxBuf[0];
        return true;
    }
    return false;
}