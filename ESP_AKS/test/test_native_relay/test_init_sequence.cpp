#include <unity.h>

#include "RelayManager.h"
#include "fake_spi.h"

// ---------------------------------------------------------------------------
// SAFETY-CRITICAL: begin() sırası.
//
// (a) Y31 — IOCON ÖNCE: diğer tüm register adresleri (OLAT/IODIR) IOCON.BANK=0
//     haritasına göredir. Chip beklenmedik bir sebeple BANK=1'e düşmüşse o
//     adresler YANLIŞ register'lara yazar. IOCON'un iki adresi vardır ve
//     hangisinin geçerli olduğu BANK bitinin kendisine bağlıdır, bu yüzden
//     HER İKİSİNE de yazılır (önce 0x05, sonra 0x0A).
//
// (b) OLAT, IODIR'den ÖNCE: MCP23S17 power-on sonrası OLAT=0x00 (LOW) ile
//     başlar; IODIR önce yazılırsa pinler output'a geçer, OLAT 0x00
//     olduğundan tüm röleler bir an için ON olur (active-low donanım).
//
// Doğru sıra:
//   1. IOCON(BANK=1 adresi 0x05) = 0x00
//   2. IOCON(BANK=0 adresi 0x0A) = 0x00
//   3. OLATA = 0xFF
//   4. OLATB = 0xFF
//   5. IODIRA = 0x00 (output)
//   6. IODIRB = 0x00 (output)
// ---------------------------------------------------------------------------

void test_begin_writes_in_safe_order(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();

    TEST_ASSERT_EQUAL_size_t(6, fake_spi_write_count());

    // 1: IOCON — BANK=1 haritasındaki adres (chip BANK=1'deyse buraya düşer)
    TEST_ASSERT_EQUAL_HEX8(0x05, fake_spi_write_at(0).reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_write_at(0).value);

    // 2: IOCON — BANK=0 haritasındaki adres (normal durum)
    TEST_ASSERT_EQUAL_HEX8(0x0A, fake_spi_write_at(1).reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_write_at(1).value);

    // 3: OLATA HIGH
    TEST_ASSERT_EQUAL_HEX8(0x14, fake_spi_write_at(2).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(2).value);

    // 4: OLATB HIGH
    TEST_ASSERT_EQUAL_HEX8(0x15, fake_spi_write_at(3).reg);
    TEST_ASSERT_EQUAL_HEX8(0xFF, fake_spi_write_at(3).value);

    // 5: IODIRA → OUTPUT (yalnız bu noktada output'a geçer)
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_write_at(4).reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_write_at(4).value);

    // 6: IODIRB → OUTPUT
    TEST_ASSERT_EQUAL_HEX8(0x01, fake_spi_write_at(5).reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, fake_spi_write_at(5).value);
}

// Y31: IOCON yazımları OLAT/IODIR'den ÖNCE gelmeli. Bu, (a) maddesinin
// sırasını ayrıca sabitler — yukarıdaki test tüm sırayı doğruluyor, bu ise
// kuralın KENDİSİNİ okunur biçimde ifade eder (sıra kayarsa hangi kuralın
// kırıldığı doğrudan görünür).
void test_begin_writes_iocon_before_any_output_register(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();

    size_t firstOutputWrite = fake_spi_write_count();
    for (size_t i = 0; i < fake_spi_write_count(); ++i) {
        const uint8_t reg = fake_spi_write_at(i).reg;
        if (reg == MCP23S17_OLATA || reg == MCP23S17_OLATB ||
            reg == MCP23S17_IODIRA || reg == MCP23S17_IODIRB) {
            firstOutputWrite = i;
            break;
        }
    }

    bool sawBank1 = false, sawBank0 = false;
    for (size_t i = 0; i < firstOutputWrite; ++i) {
        if (fake_spi_write_at(i).reg == MCP23S17_IOCON_BANK1) sawBank1 = true;
        if (fake_spi_write_at(i).reg == MCP23S17_IOCON_BANK0) sawBank0 = true;
    }

    TEST_ASSERT_TRUE_MESSAGE(sawBank1,
        "IOCON'un BANK=1 adresi (0x05) ilk OLAT/IODIR yaziminda ONCE yazilmadi");
    TEST_ASSERT_TRUE_MESSAGE(sawBank0,
        "IOCON'un BANK=0 adresi (0x0A) ilk OLAT/IODIR yaziminda ONCE yazilmadi");
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
