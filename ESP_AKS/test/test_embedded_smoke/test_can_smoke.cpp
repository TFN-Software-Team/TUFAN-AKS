#include <unity.h>

#include "CanManager.h"
#include "SystemConfig.h"

// twai_driver_install + twai_start: ESP32 dahili TWAI controller'ını
// configure eder. Harici transceiver/bus olmasa bile install ve start
// ESP_OK döner (TX denemesi yapılana kadar bus error tetiklenmez).
void test_can_begin_returns_true(void) {
    // Static — testten sonra hayatta kalmasın diye function-local ama
    // CanManager destructor TWAI'yi uninstall etmediği için tek call OK.
    static CanManager s_can(CAN_TX_PIN, CAN_RX_PIN);
    bool ok = s_can.begin();
    TEST_ASSERT_TRUE(ok);
}
