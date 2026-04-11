#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

// SystemConfig.h
// Centralized configuration for pin assignments, timeouts, and other constants
// Used across multiple modules for consistency

// --- Includes ---
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"

// --- CAN (TJA1050 transceiver) ---
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// --- Nextion HMI (UART) ---
#define HMI_UART_NUM UART_NUM_1
// Not: J8 konnektöründe screen_TX'in ekranın mı yoksa ESP'nin mi TX'i olduğuna göre
// aşağıdaki 32 ve 33 yer değiştirebilir, ancak donanım pinleri 32 ve 33'tür.
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
// OUT0–OUT9 available, assign as hardware is finalized
#define RELAY_CH_PRE_CHARGE 0  // TODO: confirm with hardware team
#define RELAY_CH_MAIN_POS 1    // TODO: confirm
#define RELAY_CH_MAIN_NEG 2    // TODO: confirm
// OUT3–OUT9 reserved for future use

// --- UKS Emergency Stop Command (LoRa packet ID) ---
#define UKS_CMD_EMERGENCY_STOP 0xA1
#define UKS_CMD_START 0xA2
#define UKS_CMD_STOP 0xA3

#endif  // SYSTEM_CONFIG_H
