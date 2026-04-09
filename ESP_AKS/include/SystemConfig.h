#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

// Change these pin definitions as needed for your specific hardware setup

#include "driver/gpio.h"
#include "driver/uart.h"

#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4
#define HMI_UART_NUM UART_NUM_1
#define HMI_TX_PIN GPIO_NUM_17
#define HMI_RX_PIN GPIO_NUM_16
#define SPI_HOST_NUM VSPI_HOST
#define SPI_CS_PIN GPIO_NUM_15

#endif  // SYSTEM_CONFIG_H
