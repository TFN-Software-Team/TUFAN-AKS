#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

// SystemConfig.h
// Centralized configuration for pin assignments, timeouts, and other constants
// Used across multiple modules for consistency

// --- Includes ---
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"

// --- CAN Message IDs ---
#define CAN_ID_TORQUE_CMD 0x100    // AKS → Motor Driver
#define CAN_ID_MOTOR_STATUS 0x200  // Motor Driver → AKS
#define CAN_ID_BMS_STATUS 0x300    // Legacy BMS → AKS
#define CAN_ID_BMS_CONFIG 0xE000   // Lithium Balance BMS config frame
#define CAN_ID_BMS_LIVE 0xE001     // Lithium Balance BMS live frame

// --- CAN (TJA1050 transceiver) ---
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// --- Nextion HMI (UART) ---
#define HMI_UART_NUM UART_NUM_1
// Not: J8 konnektöründe screen_TX'in ekranın mı yoksa ESP'nin mi TX'i olduğuna
// göre aşağıdaki 32 ve 33 yer değiştirebilir, ancak donanım pinleri 32 ve
// 33'tür.
#define HMI_TX_PIN GPIO_NUM_33  // Şemadaki screen_RX (ESP TX -> Ekran RX)
#define HMI_RX_PIN GPIO_NUM_32  // Şemadaki screen_TX (Ekran TX -> ESP RX)

// --- LoRa E32-433T30D (UART & Kontrol) ---
#define LORA_UART_NUM UART_NUM_2
#define LORA_TX_PIN GPIO_NUM_16   // ESP TX -> Şemadaki LR_RXD (IO16)
#define LORA_RX_PIN GPIO_NUM_17   // ESP RX <- Şemadaki LR_TXD (IO17)
#define LORA_AUX_PIN GPIO_NUM_35  // Şemada AUX IO35'e bağlı (Giriş)
#define LORA_M0_PIN GPIO_NUM_25   // Şemadaki MO (IO25)
#define LORA_M1_PIN GPIO_NUM_26   // Şemadaki M1 (IO26)
#define LORA_UART_BAUD 9600       // E32 default baud
#define LORA_TX_PERIOD_MS 200     // 5 Hz telemetry uplink
#define LORA_MODE_NORMAL_M0_LEVEL 0
#define LORA_MODE_NORMAL_M1_LEVEL 0
// Planned startup mode for E32:
// M0 = 0, M1 = 0 -> normal transparent UART mode.
// AUX should be used as a readiness gate before TX once the GPIO init
// sequence is added in the LoRa task startup path.

// --- MCP23S17 I/O Expander (SPI) → Relays ---
#define RELAY_SPI_HOST SPI2_HOST
#define RELAY_SPI_MOSI GPIO_NUM_23
#define RELAY_SPI_MISO GPIO_NUM_19
#define RELAY_SPI_CLK GPIO_NUM_18
#define RELAY_SPI_CS GPIO_NUM_15

// --- MCP23S17 Relay Channel Assignments ---
// All relay outputs are active-low and currently reserved for the positive
// contactor bank. The software mapping is stable, but the final harness /
// physical load assignment for each channel still needs hardware validation.
// Keep this table synchronized with the wiring document before replacing the
// placeholder descriptions below.
#define RELAY_CH_POS_0 0  // Positive contactor output 0 (physical load TBD)
#define RELAY_CH_POS_1 1  // Positive contactor output 1 (physical load TBD)
#define RELAY_CH_POS_2 2  // Positive contactor output 2 (physical load TBD)
#define RELAY_CH_POS_3 3  // Positive contactor output 3 (physical load TBD)
#define RELAY_CH_POS_4 4  // Positive contactor output 4 (physical load TBD)
#define RELAY_CH_POS_5 5  // Positive contactor output 5 (physical load TBD)
#define RELAY_CH_POS_6 6  // Positive contactor output 6 (physical load TBD)
#define RELAY_CH_POS_7 7  // Positive contactor output 7 (physical load TBD)
#define RELAY_CH_POS_8 8  // Positive contactor output 8 (physical load TBD)
#define RELAY_CH_POS_9 9  // Positive contactor output 9 (physical load TBD)

#define RELAY_TOTAL_CHANNELS 10

// --- UKS Emergency Stop Command (LoRa packet ID) ---
#define UKS_CMD_EMERGENCY_STOP 0xA1
#define UKS_CMD_START 0xA2
#define UKS_CMD_STOP 0xA3

#endif  // SYSTEM_CONFIG_H
