#pragma once
// Native test stub for driver/uart.h
// SystemConfig.h dahil edildiğinde include zincirinin kırılmaması için var.
// Faz 4: Telemetry.cpp `uart_write_bytes` çağırdığından bu fonksiyonun ve
// ilgili portların minimal tanımları gerekiyor.
#include <cstddef>
#include <cstdint>

typedef int uart_port_t;

#define UART_NUM_0 ((uart_port_t)0)
#define UART_NUM_1 ((uart_port_t)1)
#define UART_NUM_2 ((uart_port_t)2)

#ifdef __cplusplus
extern "C" {
#endif

// Gerçek imza: int uart_write_bytes(uart_port_t, const void*, size_t)
int uart_write_bytes(uart_port_t uart_num, const void* src, size_t size);
int uart_read_bytes(uart_port_t uart_num, void* buf, uint32_t length, uint32_t ticks_to_wait);

typedef struct {
    int baud_rate;
    int data_bits;
    int parity;
    int stop_bits;
    int flow_ctrl;
    int rx_flow_ctrl_thresh;
    int source_clk;
} uart_config_t;

#define UART_DATA_8_BITS 0
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 0
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1

int uart_param_config(uart_port_t uart_num, const uart_config_t *uart_config);
int uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
int uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, void* uart_queue, int intr_alloc_flags);

#ifdef __cplusplus
}
#endif
