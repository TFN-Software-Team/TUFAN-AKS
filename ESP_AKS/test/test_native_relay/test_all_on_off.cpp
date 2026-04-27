#include <unity.h>

#include "RelayManager.h"
#include "fake_spi.h"

namespace {
void primeRelay() {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();
    fake_spi_reset();
}
}  // namespace

// ---------------------------------------------------------------------------
// allOn(): 10 kanal logical=1 → hw = ~0x03FF = 0xFC00.
//   OLATA = 0x00, OLATB = 0xFC
// ---------------------------------------------------------------------------
void test_allOn_drives_all_active_low(void) {
    primeRelay();
    RelayManager::instance().allOn();

    TEST_ASSERT_EQUAL_size_t(2, fake_spi_write_count());
    TEST_ASSERT_EQUAL_HEX8(0x14, fake_spi_write_at(0).reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_write_at(0).value);
    TEST_ASSERT_EQUAL_HEX8(0x15, fake_spi_write_at(1).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFC, fake_spi_write_at(1).value);
}

void test_allOn_sets_all_state_bits(void) {
    primeRelay();
    RelayManager::instance().allOn();
    for (uint8_t ch = 0; ch < 10; ++ch) {
        TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(ch));
    }
}

// ---------------------------------------------------------------------------
// allOff(): 0xFFFF → tüm pinler HIGH.
// ---------------------------------------------------------------------------
void test_allOff_drives_all_high(void) {
    primeRelay();
    RelayManager::instance().allOff();

    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(0).value);
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(1).value);
}

void test_allOff_clears_state(void) {
    primeRelay();
    RelayManager::instance().allOn();
    RelayManager::instance().allOff();
    for (uint8_t ch = 0; ch < 10; ++ch) {
        TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(ch));
    }
}

// allOff() begin() öncesi: writeRegister s_spiDev nullptr olduğundan early
// return yapar; SPI yazısı oluşmaz ama logical state defansif olarak
// sıfırlanır.
void test_allOff_before_begin_clears_state_but_no_spi(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().allOff();

    TEST_ASSERT_EQUAL_size_t(0, fake_spi_write_count());
    for (uint8_t ch = 0; ch < 10; ++ch) {
        TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(ch));
    }
}

void test_allOn_before_begin_is_noop(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().allOn();
    // allOn s_initialized kontrolü yapar, hiçbir SPI yazısı olmamalı.
    TEST_ASSERT_EQUAL_size_t(0, fake_spi_write_count());
}
