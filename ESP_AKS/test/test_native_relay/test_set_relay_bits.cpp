#include <unity.h>

#include "RelayManager.h"
#include "fake_spi.h"

namespace {
void primeRelay() {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();
    fake_spi_reset();  // begin()'in init writes'larını izole etmek için temizle
}
}  // namespace

// ---------------------------------------------------------------------------
// Active-low logic: setRelay(channel, true) hardware'de o bit'i LOW yapar.
// MCP23S17_OLATA = 0x14, MCP23S17_OLATB = 0x15.
// ---------------------------------------------------------------------------

void test_setRelay_channel0_on_drives_olata_low_bit(void) {
    primeRelay();
    RelayManager::instance().setRelay(0, true);

    // setRelay her çağrıda 2 SPI yazısı yapar: OLATA + OLATB
    TEST_ASSERT_EQUAL_size_t(2, fake_spi_write_count());
    TEST_ASSERT_EQUAL_HEX8(0x14, fake_spi_write_at(0).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFE, fake_spi_write_at(0).value);  // bit0 LOW
    TEST_ASSERT_EQUAL_HEX8(0x15, fake_spi_write_at(1).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(1).value);
}

void test_setRelay_channel0_off_restores_olata_high(void) {
    primeRelay();
    RelayManager::instance().setRelay(0, true);
    fake_spi_reset();

    RelayManager::instance().setRelay(0, false);

    TEST_ASSERT_EQUAL_HEX8(0x14, fake_spi_write_at(0).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(0).value);
}

void test_setRelay_channel8_drives_olatb_low_bit(void) {
    primeRelay();
    RelayManager::instance().setRelay(8, true);

    TEST_ASSERT_EQUAL_HEX8(0x14, fake_spi_write_at(0).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(0).value);
    TEST_ASSERT_EQUAL_HEX8(0x15, fake_spi_write_at(1).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFE, fake_spi_write_at(1).value);  // OLATB bit0
}

void test_setRelay_channel9_drives_olatb_bit1_low(void) {
    primeRelay();
    RelayManager::instance().setRelay(9, true);

    TEST_ASSERT_EQUAL_HEX8(0xFD, fake_spi_write_at(1).value);  // OLATB bit1 LOW
}

void test_setRelay_out_of_range_does_not_write_spi(void) {
    primeRelay();
    RelayManager::instance().setRelay(10, true);
    RelayManager::instance().setRelay(255, true);
    TEST_ASSERT_EQUAL_size_t(0, fake_spi_write_count());
}

void test_setRelay_before_begin_is_noop(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    // begin() çağrılmadı → s_initialized=false
    RelayManager::instance().setRelay(0, true);
    TEST_ASSERT_EQUAL_size_t(0, fake_spi_write_count());
}

void test_getRelayState_reflects_setRelay(void) {
    primeRelay();
    RelayManager::instance().setRelay(3, true);
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(3));
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(4));
}

void test_getRelayState_out_of_range_returns_false(void) {
    primeRelay();
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(10));
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(99));
}

void test_setRelay_multiple_channels_accumulates_state(void) {
    primeRelay();
    RelayManager::instance().setRelay(0, true);
    RelayManager::instance().setRelay(1, true);
    RelayManager::instance().setRelay(2, true);
    fake_spi_reset();

    // 4. çağrı yalnızca bit3'ü ekler — diğerleri set kalır.
    RelayManager::instance().setRelay(3, true);

    // OLATA = ~0b0000_1111 = 0xF0
    TEST_ASSERT_EQUAL_HEX8(0xF0, fake_spi_write_at(0).value);
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(1).value);  // OLATB değişmedi
}
