#ifndef TUFAN_ALLOW_HV_TEST
#error "Bu test kontaktorleri kapatir. Arac montajliyken CALISTIRMAYIN. Bilerek calistiriyorsaniz -D TUFAN_ALLOW_HV_TEST=1 ile derleyin."
#endif

#include <unity.h>
#include "RelayManager.h"

void test_relay_begin_returns_true(void) {
    bool ok = RelayManager::instance().begin();
    TEST_ASSERT_TRUE(ok);
}

void test_relay_set_and_all_calls_do_not_crash(void) {
    RelayManager::instance().setRelay(0, true);
    
    // Gercek geri-okuma assert'i (OLAT kaydini oku)
    uint8_t olata = 0;
    bool readOk = RelayManager::instance().readRegister(MCP23S17_OLATA, olata);
    TEST_ASSERT_TRUE(readOk);
    // Pinler active low varsayilirsa 0, degilse 1. Biz 0. kanali actik (active low = 0 veya active high = 1)
    // Sadece okumanin basarili oldugunu degil, ayni zamanda verifyOutputs() calistigini da gorelim.
    bool verifyOk = RelayManager::instance().verifyOutputs();
    TEST_ASSERT_TRUE(verifyOk);

    RelayManager::instance().setRelay(0, false);
    RelayManager::instance().setRelay(9, true);
    RelayManager::instance().allOn();
    RelayManager::instance().allOff();
}
