#include <unity.h>

#include "RelayManager.h"
#include "fake_spi.h"

// ---------------------------------------------------------------------------
// SAFETY-CRITICAL: begin() OLAT register'larını IODIR register'larından ÖNCE
// yazmalıdır. MCP23S17 power-on sonrası OLAT=0x00 (LOW) ile başlar; eğer
// IODIR önce yazılırsa pinler output'a geçer, OLAT 0x00 olduğundan tüm
// röleler bir an için ON olur. Doğru sıra:
//   1. OLATA = 0xFF
//   2. OLATB = 0xFF
//   3. IODIRA = 0x00 (output)
//   4. IODIRB = 0x00 (output)
// ---------------------------------------------------------------------------

void test_begin_writes_in_safe_order(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();

    TEST_ASSERT_EQUAL_size_t(4, fake_spi_write_count());

    // 1: OLATA HIGH
    TEST_ASSERT_EQUAL_HEX8(0x14, fake_spi_write_at(0).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(0).value);

    // 2: OLATB HIGH
    TEST_ASSERT_EQUAL_HEX8(0x15, fake_spi_write_at(1).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(1).value);

    // 3: IODIRA → OUTPUT (yalnız bu noktada output'a geçer)
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_write_at(2).reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_write_at(2).value);

    // 4: IODIRB → OUTPUT
    TEST_ASSERT_EQUAL_HEX8(0x01, fake_spi_write_at(3).reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_write_at(3).value);
}

void test_begin_returns_true_on_success(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    bool ok = RelayManager::instance().begin();
    TEST_ASSERT_TRUE(ok);
}

void test_begin_initializes_state_to_zero(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();
    for (uint8_t ch = 0; ch < 10; ++ch) {
        TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(ch));
    }
}
