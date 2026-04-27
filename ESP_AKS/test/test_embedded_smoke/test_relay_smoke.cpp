#include <unity.h>

#include "RelayManager.h"

// SPI bus init + MCP23S17 device add — fiziksel chip bağlı olmasa bile
// ESP32 SPI master sürücüsü ESP_OK döner; bu yüzden begin() true olmalı.
void test_relay_begin_returns_true(void) {
    bool ok = RelayManager::instance().begin();
    TEST_ASSERT_TRUE(ok);
}

// begin() sonrası setRelay/allOn/allOff çağrıları sürücü hatası veya kilit
// olmadan dönmelidir. Sayaç yok — yaşam (no crash, no hang) yeterli smoke.
void test_relay_set_and_all_calls_do_not_crash(void) {
    RelayManager::instance().setRelay(0, true);
    RelayManager::instance().setRelay(0, false);
    RelayManager::instance().setRelay(9, true);
    RelayManager::instance().allOn();
    RelayManager::instance().allOff();
    TEST_ASSERT_TRUE(true);
}
