#include <unity.h>

#include "RelayManager.h"
#include "SystemConfig.h"
#include "../test_native_relay/fake_spi.h"

// ===========================================================================
// RELAY_ROLES_ASSIGNED=1 iken GERÇEK RelayManager sürücüsünün bank-maskesi
// davranışı ([env:native_roles]): allOn/allOff yalnız RELAY_CONTACTOR_BANK_
// MASK (0x35B — flaşör 5 + fan 7 + far 2 HARİÇ) kanallarını sürer; flaşör,
// fan ve far kanallarının son yazılan durumu shadow'da korunur ve
// verifyOutputs geri-okuması yeni maskeyle tutarlıdır. Varsayılan (0x3FF)
// davranış regresyonları test_native_relay'de.
// ===========================================================================

#if !RELAY_ROLES_ASSIGNED
#error "Bu suite yalniz RELAY_ROLES_ASSIGNED=1 ile derlenmeli (env:native_roles)"
#endif

namespace {
void primeRelay() {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();
    fake_spi_reset();
}
}  // namespace

// allOff, yanık flaşörü SÖNDÜRMEZ: kanal 5 (OLATA bit5) LOW (aktif) kalır,
// bank kanalları (0,1,3,4,6,8,9) HIGH (açık) olur.
void test_allOff_preserves_flasher_channel(void) {
    primeRelay();
    RelayManager::instance().setRelay(RELAY_CH_FLASHER, true);  // flaşör yanık
    RelayManager::instance().allOn();                            // bank kapalı
    fake_spi_reset();

    RelayManager::instance().allOff();

    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_FLASHER));
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch)) {
            TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(ch));
        }
    }
    // Donanım seviyesi: mantıksal state=0x020 (yalnız flaşör yanık). Beklenen
    // pin deseni hwFromLogical'dan TÜRETİLİR — sabit yazılmaz, çünkü desen
    // RELAY_INVERT_MASK'e (NC klemensli kanallar) bağlıdır. Maske 0 iken
    // 0xDF/0xFF, fan tersken (0x080) 0x5F/0xFF olur; test ikisinde de geçer.
    constexpr uint16_t hwFlasherOnly = RelayManager::hwFromLogical(1u << RELAY_CH_FLASHER);
    TEST_ASSERT_EQUAL_HEX8(hwFlasherOnly & 0xFF, fake_spi_get_reg(MCP23S17_OLATA));
    TEST_ASSERT_EQUAL_HEX8((hwFlasherOnly >> 8) & 0xFF, fake_spi_get_reg(MCP23S17_OLATB));
    // Flaşör pini yükü ÇALIŞTIRAN seviyede olmalı (maskeden bağımsız iddia).
    TEST_ASSERT_EQUAL_HEX8(
        (RELAY_INVERT_MASK & (1u << RELAY_CH_FLASHER)) ? (1u << RELAY_CH_FLASHER) : 0u,
        fake_spi_get_reg(MCP23S17_OLATA) & (1u << RELAY_CH_FLASHER));
}

// allOn, sönük flaşörü/fanı/farı YAKMAZ: yalnız bank kanalları
// (0,1,3,4,6,8,9) kapanır — bank dışı kanallar (2 far, 5 flaşör, 7 fan)
// etkilenmez.
void test_allOn_does_not_energize_flasher(void) {
    primeRelay();

    RelayManager::instance().allOn();

    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_FLASHER));
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_FAN));
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_HEADLIGHT));
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch)) {
            TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(ch));
        }
    }
    // state=0x35B (bank kapalı, bank dışı üç kanal sönük) → beklenen pin deseni
    // yine hwFromLogical'dan türetilir (bkz. yukarıdaki gerekçe).
    constexpr uint16_t hwBankOnly = RelayManager::hwFromLogical(RELAY_CONTACTOR_BANK_MASK);
    TEST_ASSERT_EQUAL_HEX8(hwBankOnly & 0xFF, fake_spi_get_reg(MCP23S17_OLATA));
    TEST_ASSERT_EQUAL_HEX8((hwBankOnly >> 8) & 0xFF, fake_spi_get_reg(MCP23S17_OLATB));
}

// allOff, yanık fan ve farı SÖNDÜRMEZ (bank DIŞI kanallar): sıcak batarya
// soğutması / sürücü far kontrolü güvenlik açmasıyla kesilmez.
void test_allOff_preserves_fan_and_headlight(void) {
    primeRelay();
    RelayManager::instance().setRelay(RELAY_CH_FAN, true);
    RelayManager::instance().setRelay(RELAY_CH_HEADLIGHT, true);
    RelayManager::instance().allOn();
    fake_spi_reset();

    RelayManager::instance().allOff();

    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_FAN));
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_HEADLIGHT));
    for (uint8_t ch = 0; ch < RELAY_TOTAL_CHANNELS; ++ch) {
        if (RELAY_CONTACTOR_BANK_MASK & (1u << ch)) {
            TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(ch));
        }
    }
}

// HV- (HVNEG, kanal 1) S2 (kanal 4) ile BİRLİKTE açılıp kapanır: ikisi de
// sürüş bankı (RELAY_DRIVE_BANK_MASK) üyesi. allOn → ikisi kapalı; allOff →
// ikisi açık.
void test_hvneg_switches_with_s2(void) {
    primeRelay();

    RelayManager::instance().allOn();
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_S2_DRIVE));
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_HVNEG));

    RelayManager::instance().allOff();
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_S2_DRIVE));
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_HVNEG));
}

// verifyOutputs, maske dışı (flaşör) kanal dahil TÜM shadow'u geri-okumayla
// karşılaştırır — flaşör yanıkken allOff sonrası doğrulama TUTARLI kalmalı
// (fault latch'lenmemeli).
void test_verify_consistent_with_mask_after_allOff(void) {
    primeRelay();
    RelayManager::instance().setRelay(RELAY_CH_FLASHER, true);
    RelayManager::instance().allOff();
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());

    fake_spi_reset();
    TEST_ASSERT_TRUE(RelayManager::instance().verifyOutputs());
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());
    TEST_ASSERT_EQUAL_size_t(0, fake_spi_write_count());  // yalnız READ
}

// Maske sözleşmesi (derleme sabitleri): kontaktör bankında flaşör + fan + far
// dışarıda, S1 + S2 + HV- + yedekler içeride; sürüş bankı ise YALNIZ S2 + HV-
// (kablosuz yedekler READY'de enerjilenmez — asimetri bilinçlidir: allOff
// yedekleri de açar, READY onları kapatmaz).
void test_mask_contract_values(void) {
    TEST_ASSERT_EQUAL_HEX16(0x35B, RELAY_CONTACTOR_BANK_MASK);
    TEST_ASSERT_EQUAL_HEX16(0x012, RELAY_DRIVE_BANK_MASK);
    TEST_ASSERT_EQUAL_UINT8(4, RELAY_CH_S2_DRIVE);
    TEST_ASSERT_EQUAL_UINT8(1, RELAY_CH_HVNEG);
    TEST_ASSERT_EQUAL_UINT8(2, RELAY_CH_HEADLIGHT);
    TEST_ASSERT_EQUAL_UINT8(7, RELAY_CH_FAN);
    TEST_ASSERT_EQUAL_UINT8(0, RELAY_CH_S1_CHARGE);
    TEST_ASSERT_EQUAL_UINT8(5, RELAY_CH_FLASHER);
    // Bank DIŞI: far, fan, flaşör. Bank İÇİ: S1, S2, HV-.
    TEST_ASSERT_EQUAL_UINT(0, RELAY_CONTACTOR_BANK_MASK & (1u << RELAY_CH_HEADLIGHT));
    TEST_ASSERT_EQUAL_UINT(0, RELAY_CONTACTOR_BANK_MASK & (1u << RELAY_CH_FAN));
    TEST_ASSERT_EQUAL_UINT(0, RELAY_CONTACTOR_BANK_MASK & (1u << RELAY_CH_FLASHER));
    TEST_ASSERT_TRUE(RELAY_DRIVE_BANK_MASK & (1u << RELAY_CH_HVNEG));

    // Sürüş bankı kontaktör bankının ALT KÜMESİ: READY'de kapatılan her kanalı
    // allOff güvenlik açması da açabilmeli (şartname 8.2.a.vi).
    TEST_ASSERT_EQUAL_UINT(0, RELAY_DRIVE_BANK_MASK & ~(unsigned)RELAY_CONTACTOR_BANK_MASK);
    // S1 sürüş bankının DIŞINDA (8.2.a.vii), kontaktör bankının İÇİNDE.
    TEST_ASSERT_EQUAL_UINT(0, RELAY_DRIVE_BANK_MASK & (1u << RELAY_CH_S1_CHARGE));
    // Kablosuz yedekler: allOff'un açtığı bankın İÇİNDE, READY'nin kapattığı
    // sürüş bankının DIŞINDA (bilinçli asimetri).
    const uint16_t spares = (1u << RELAY_CH_SPARE_3) | (1u << RELAY_CH_SPARE_6) |
                            (1u << RELAY_CH_SPARE_8) | (1u << RELAY_CH_SPARE_9);
    TEST_ASSERT_EQUAL_HEX16(spares, RELAY_CONTACTOR_BANK_MASK & spares);
    TEST_ASSERT_EQUAL_UINT(0, RELAY_DRIVE_BANK_MASK & spares);
}

// ===========================================================================
// SORUN 2 (2026-07-29) — NC klemensli fan kanalının polarite sözleşmesi.
// Saha bulgusu: ekranda 32 °C okunurken fan dönüyordu. Yazılım fanı KAPALI
// komutluyordu; fan NC (normalde kapalı) klemense bağlı olduğu için röle
// enerjisizken yük ÇALIŞIYORDU. RELAY_INVERT_MASK bu kanalın mantıksal→pin
// çevrimini tersler; aşağıdaki testler o sözleşmeyi kilitler.
// ===========================================================================

// Fan NC olarak işaretliyse RELAY_INVERT_MASK yalnız fan kanalını içermeli.
void test_invert_mask_contract(void) {
#if RELAY_CH_FAN_NC_WIRED
    TEST_ASSERT_EQUAL_HEX16(1u << RELAY_CH_FAN, RELAY_INVERT_MASK);
#else
    TEST_ASSERT_EQUAL_HEX16(0, RELAY_INVERT_MASK);
#endif
    // GÜVENLİK: hiçbir kontaktör terslenemez — terslenirse allOff (güvenlik
    // açması) o kontaktörü KAPATIRDI (şartname 8.2.a.vi'nin tam tersi).
    TEST_ASSERT_EQUAL_UINT(0, RELAY_INVERT_MASK & (unsigned)RELAY_CONTACTOR_BANK_MASK);
}

// begin() sonrası fan pini "yük KAPALI" seviyesinde olmalı. NC kanalda bu
// LOW'dur (bobin enerjili → NC kontağı açık → fan durur), sabit 0xFF DEĞİL.
// Bu, 32 °C'de fanın boot anından itibaren dönmemesinin donanım seviyesi
// garantisidir.
void test_begin_leaves_fan_load_off(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();

    const uint8_t olatA = fake_spi_get_reg(MCP23S17_OLATA);
    const uint8_t fanBit = olatA & (1u << RELAY_CH_FAN);
#if RELAY_CH_FAN_NC_WIRED
    TEST_ASSERT_EQUAL_HEX8(0, fanBit);  // NC: yük KAPALI = pin LOW
#else
    TEST_ASSERT_EQUAL_HEX8(1u << RELAY_CH_FAN, fanBit);  // NO: yük KAPALI = pin HIGH
#endif
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_FAN));
}

// setRelay(FAN, true/false) mantıksal durumu DEĞİŞTİRMEDEN raporlar (üst
// katmanlar hep YÜK'ün durumunu konuşur) ama pin seviyesi NC'de terstir.
void test_fan_pin_polarity_follows_invert_mask(void) {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();

    RelayManager::instance().setRelay(RELAY_CH_FAN, true);  // fan ÇALIŞSIN
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_FAN));
    const uint8_t onBit = fake_spi_get_reg(MCP23S17_OLATA) & (1u << RELAY_CH_FAN);

    RelayManager::instance().setRelay(RELAY_CH_FAN, false);  // fan DURSUN
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(RELAY_CH_FAN));
    const uint8_t offBit = fake_spi_get_reg(MCP23S17_OLATA) & (1u << RELAY_CH_FAN);

    // İki durum farklı pin seviyesi üretmeli (kanal gerçekten sürülüyor).
    TEST_ASSERT_NOT_EQUAL(onBit, offBit);
#if RELAY_CH_FAN_NC_WIRED
    TEST_ASSERT_EQUAL_HEX8(1u << RELAY_CH_FAN, onBit);  // NC: ON = pin HIGH
    TEST_ASSERT_EQUAL_HEX8(0, offBit);
#else
    TEST_ASSERT_EQUAL_HEX8(0, onBit);  // NO (active-low): ON = pin LOW
    TEST_ASSERT_EQUAL_HEX8(1u << RELAY_CH_FAN, offBit);
#endif
}

// Geri-okuma doğrulaması terslenmiş kanalla TUTARLI olmalı: fan çalışırken
// verifyOutputs uyuşmazlık sanıp actuator fault LATCH'LEMEMELİ.
void test_verify_consistent_with_inverted_fan_channel(void) {
    primeRelay();
    RelayManager::instance().setRelay(RELAY_CH_FAN, true);
    RelayManager::instance().allOff();
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(RELAY_CH_FAN));
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());

    fake_spi_reset();
    TEST_ASSERT_TRUE(RelayManager::instance().verifyOutputs());
    TEST_ASSERT_FALSE(RelayManager::instance().hasActuatorFault());
}

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_invert_mask_contract);
    RUN_TEST(test_begin_leaves_fan_load_off);
    RUN_TEST(test_fan_pin_polarity_follows_invert_mask);
    RUN_TEST(test_verify_consistent_with_inverted_fan_channel);
    RUN_TEST(test_allOff_preserves_flasher_channel);
    RUN_TEST(test_allOn_does_not_energize_flasher);
    RUN_TEST(test_allOff_preserves_fan_and_headlight);
    RUN_TEST(test_hvneg_switches_with_s2);
    RUN_TEST(test_verify_consistent_with_mask_after_allOff);
    RUN_TEST(test_mask_contract_values);
    return UNITY_END();
}
