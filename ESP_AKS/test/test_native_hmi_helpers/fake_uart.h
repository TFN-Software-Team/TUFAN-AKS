#pragma once
// Native testler için UART TX yakalayıcı (test_native_telemetry'deki ile
// fonksiyonel olarak aynı; PIO her test dizinini ayrı binary derlediği için
// burada tekrar tanımlanır).
#include <cstddef>

const char* fake_uart_get_buffer(void);
size_t      fake_uart_get_size(void);
void        fake_uart_reset(void);
