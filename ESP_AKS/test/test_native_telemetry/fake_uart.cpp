// uart_write_bytes implementasyonu — gerçek UART donanımına yazmak yerine
// bir vector'a biriktirir, böylece testler tam string içeriğini doğrulayabilir.
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
