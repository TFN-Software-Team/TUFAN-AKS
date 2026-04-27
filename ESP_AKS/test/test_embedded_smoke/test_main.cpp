// Faz 7 — Embedded smoke testleri.
//
// Bu testler GERÇEK ESP32 üzerinde çalışır. Amaç saf mantık değil; native
// suite'lerinin stub'ladığı ESP-IDF driver'larının (SPI, TWAI, UART) gerçek
// donanımda init/lifecycle yapabildiğini doğrulamak.
//
// Çalıştırma: bağlı bir ESP32 dev board ve doğru COM portu gerekir.
// Gerçek MCP23S17 / CAN transceiver / LoRa modülü TAKILI OLMASA bile
// driver init başarılı olur (peripheral init MCU içinde local'dir).
//
// Komut: `pio test -e esp32dev -f test_embedded_smoke`
#include <unity.h>

extern void test_relay_begin_returns_true(void);
extern void test_relay_set_and_all_calls_do_not_crash(void);

extern void test_can_begin_returns_true(void);

extern void test_telemetry_begin_and_send(void);

void setUp(void) {}
void tearDown(void) {}

extern "C" void app_main(void) {
    UNITY_BEGIN();

    // RelayManager — SPI bus + MCP23S17 init (chip yoksa bile bus init OK)
    RUN_TEST(test_relay_begin_returns_true);
    RUN_TEST(test_relay_set_and_all_calls_do_not_crash);

    // CanManager — TWAI install + start
    RUN_TEST(test_can_begin_returns_true);

    // Telemetry — UART2 driver install + sendStatus
    RUN_TEST(test_telemetry_begin_and_send);

    UNITY_END();
}
