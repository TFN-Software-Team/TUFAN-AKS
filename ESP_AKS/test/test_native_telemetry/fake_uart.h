#pragma once
// Native testler için UART TX yakalayıcı.  uart_write_bytes çağrılarındaki
// tüm byte'lar global bir buffer'a biriktirilir; testler içeriği inceler.
#include <cstddef>

const char* fake_uart_get_buffer(void);
size_t      fake_uart_get_size(void);
void        fake_uart_reset(void);
