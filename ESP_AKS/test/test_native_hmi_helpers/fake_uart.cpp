#include "fake_uart.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "driver/uart.h"

namespace {
std::vector<char> s_buffer;
std::string s_view;
}  // namespace

extern "C" int uart_write_bytes(uart_port_t /*uart_num*/, const void* src,
                                size_t size) {
    const char* p = static_cast<const char*>(src);
    s_buffer.insert(s_buffer.end(), p, p + size);
    return static_cast<int>(size);
}

extern "C" int uart_read_bytes(uart_port_t uart_num, void* buf, uint32_t length, uint32_t ticks_to_wait) {
    return 0; // nothing to read
}

extern "C" int uart_param_config(uart_port_t uart_num, const uart_config_t *uart_config) {
    return 0; // ESP_OK
}

extern "C" int uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num) {
    return 0;
}

extern "C" int uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, void* uart_queue, int intr_alloc_flags) {
    return 0;
}

const char* fake_uart_get_buffer(void) {
    s_view.assign(s_buffer.data(), s_buffer.size());
    return s_view.c_str();
}

size_t fake_uart_get_size(void) {
    return s_buffer.size();
}

void fake_uart_reset(void) {
    s_buffer.clear();
    s_view.clear();
}
