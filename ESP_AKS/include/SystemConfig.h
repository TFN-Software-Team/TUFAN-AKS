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
#define CAN_ID_BMS_STATUS 0x300    // BMS → AKS

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

// --- MCP23S17 I/O Expander (SPI) → Relays ---
#define RELAY_SPI_HOST SPI2_HOST
#define RELAY_SPI_MOSI GPIO_NUM_23
#define RELAY_SPI_MISO GPIO_NUM_19
#define RELAY_SPI_CLK GPIO_NUM_18
#define RELAY_SPI_CS GPIO_NUM_15

// --- MCP23S17 Relay Channel Assignments ---
// --- Relay Channel Assignments (all positive contactors) ---
#define RELAY_CH_POS_0 0
#define RELAY_CH_POS_1 1
#define RELAY_CH_POS_2 2
#define RELAY_CH_POS_3 3
#define RELAY_CH_POS_4 4
#define RELAY_CH_POS_5 5
#define RELAY_CH_POS_6 6
#define RELAY_CH_POS_7 7
#define RELAY_CH_POS_8 8
#define RELAY_CH_POS_9 9

#define RELAY_TOTAL_CHANNELS 10

// --- UKS Emergency Stop Command (LoRa packet ID) ---
#define UKS_CMD_EMERGENCY_STOP 0xA1
#define UKS_CMD_START 0xA2
#define UKS_CMD_STOP 0xA3

#endif  // SYSTEM_CONFIG_H
