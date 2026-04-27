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

#ifdef __cplusplus
}
#endif
