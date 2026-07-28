#include <unity.h>

#include "HMITouchParser.h"
#include "SystemConfig.h"

// ===========================================================================
// HMI KOMUT SOZLESMESI — NUMARA KILIDI
//
// Neden bu dosya var:
// Documents/HMI_Field_Map.md "Command Inputs" tablosu uzun sure KODLA CELISTI.
// Dokuman `2 = RESET`, `3 = EMERGENCY_STOP`, `4 = DRIVE_ENABLE` diyordu; kod
// ise `2 = DRIVE_ENABLE`, `3 = RESET`, `4 = EMERGENCY_STOP`. O tabloya gore
// cizilen bir ekran projesinde E-STOP butonu `0x5A 03 FC` gonderir ve firmware
// bunu RESET olarak isler — yani ACIL DURDURMA istegi, ariza kaydini temizleme
// istegine donusur. Yaris gunu bulunacak bir hata degil.
//
// Bu suite iki seyi birden kilitler:
//   1. HMI_CMD_* makrolarinin SAYISAL degerleri (derleme zamani + kosum zamani),
//   2. HMI_parseTouchByte'in her komut icin 3 baytlik cerceveyi
//      (`0x5A <CMD> <~CMD>`) dogru cozdugu.
//
// Numaralardan biri sessizce kayarsa bu testler kirilir; dokuman ve ekran
// projesi de ayni commit'te guncellenmelidir (CLAUDE.md Kural 1 ve Kural 5).
// ===========================================================================

namespace {

// ID 5 — KULLANIM DISI / REZERVE (28.07.2026 karari, bkz. SystemConfig.h
// "Komut 5"). Farin resmi kontrol yolu FIZIKSEL DUGMEDIR; src/main.cpp'deki
// `case 5` ve VcuLogic.cpp'deki HEADLIGHT_TOGGLE dali SILINDI. Bu ID'ye kasten
// bir HMI_CMD_* makrosu TANIMLANMADI.
//
// Yine de sozlesmenin parcasidir ve burada kilitlenir: sahadaki eski ekran
// projeleri hala 0x5A 05 FA gonderiyor olabilir. Bugun o cerceve `default`
// dalina dusup zararsizca yutulur; ID yeniden atanirsa AYNI cerceve yanlis
// eylemi tetikler. Asagidaki testler bu numaranin bos birakildigini korur.
constexpr uint8_t HMI_CMD_RESERVED_ID = 5;

// --- Derleme zamani kilidi -------------------------------------------------
// Bunlar makro degerleri; static_assert bir numara kaymasini daha binary
// olusmadan yakalar.
static_assert(HMI_CMD_START == 1,
              "HMI_CMD_START 1 OLMALI — ekran 'printh 5A 01 FE' gonderiyor.");
static_assert(HMI_CMD_DRIVE_ENABLE == 2,
              "HMI_CMD_DRIVE_ENABLE 2 OLMALI — ekran 'printh 5A 02 FD' gonderiyor.");
static_assert(HMI_CMD_RESET == 3,
              "HMI_CMD_RESET 3 OLMALI — ekran 'printh 5A 03 FC' gonderiyor.");
static_assert(HMI_CMD_EMERGENCY_STOP == 4,
              "HMI_CMD_EMERGENCY_STOP 4 OLMALI — ekran 'printh 5A 04 FB' "
              "gonderiyor. 3 ile karistirilirsa E-STOP butonu RESET gonderir!");
static_assert(HMI_CMD_STOP == 6,
              "HMI_CMD_STOP 6 OLMALI — ekran 'printh 5A 06 F9' gonderiyor.");

// 4 (E-STOP) ile 6 (STOP) arasindaki bosluk KASITLIDIR: ID 5 rezervedir, bos
// degil. Yeni bir komut buraya ATANMAZ — 5'e makro tanimlanmasi bu iki
// static_assert'i kirmadan mumkun olmasin diye numara komsuluk uzerinden
// kilitlenir.
static_assert(HMI_CMD_EMERGENCY_STOP + 1 == HMI_CMD_RESERVED_ID,
              "ID 5 REZERVEDIR (eski far toggle) — yeniden atanmamali.");
static_assert(HMI_CMD_RESERVED_ID + 1 == HMI_CMD_STOP,
              "ID 5 ile 6 arasinda bosluk yok — STOP 6'dir.");

// Tum komutlar birbirinden farkli olmali (kopya = iki buton ayni eylemi yapar).
static_assert(HMI_CMD_START != HMI_CMD_DRIVE_ENABLE &&
                  HMI_CMD_START != HMI_CMD_RESET &&
                  HMI_CMD_START != HMI_CMD_EMERGENCY_STOP &&
                  HMI_CMD_START != HMI_CMD_STOP &&
                  HMI_CMD_DRIVE_ENABLE != HMI_CMD_RESET &&
                  HMI_CMD_DRIVE_ENABLE != HMI_CMD_EMERGENCY_STOP &&
                  HMI_CMD_DRIVE_ENABLE != HMI_CMD_STOP &&
                  HMI_CMD_RESET != HMI_CMD_EMERGENCY_STOP &&
                  HMI_CMD_RESET != HMI_CMD_STOP &&
                  HMI_CMD_EMERGENCY_STOP != HMI_CMD_STOP,
              "HMI komut ID'leri birbirinden FARKLI olmali.");

// --- Cerceve yardimcilari --------------------------------------------------

constexpr uint8_t HMI_FRAME_HEADER = 0x5A;

// Sozlesme: checksum = ~CMD (bit tersi). Ekran tarafinda 0xFF - CMD ile ayni.
constexpr uint8_t expectedChecksum(uint8_t cmd) {
    return (uint8_t)(~cmd);
}

// Tam bir `0x5A <CMD> <~CMD>` cercevesini byte byte besler ve komutun BEKLENEN
// degerle cozuldugunu dogrular. Ilk iki byte cozum URETMEMELI (cerceve henuz
// tamamlanmadi), ucuncu byte uretmeli.
void assertFrameDecodes(uint8_t cmd) {
    HMI_TouchParserState state;
    uint8_t outCmd = 0xAA;  // komutlarin hicbiri degil — yazildigini gorelim

    TEST_ASSERT_FALSE_MESSAGE(
        HMI_parseTouchByte(HMI_FRAME_HEADER, state, outCmd),
        "Header tek basina komut URETMEMELI");
    TEST_ASSERT_FALSE_MESSAGE(
        HMI_parseTouchByte(cmd, state, outCmd),
        "Checksum gelmeden komut URETMEMELI");
    TEST_ASSERT_TRUE_MESSAGE(
        HMI_parseTouchByte(expectedChecksum(cmd), state, outCmd),
        "Gecerli checksum ile cerceve COZULMELI");
    TEST_ASSERT_EQUAL_UINT8(cmd, outCmd);
}

}  // namespace

// ===========================================================================
// 1. Makro degerleri (kosum zamani aynasi — static_assert'in gorunur hali)
// ===========================================================================

void test_cmd_ids_match_nextion_contract(void) {
    TEST_ASSERT_EQUAL_UINT8(1, HMI_CMD_START);
    TEST_ASSERT_EQUAL_UINT8(2, HMI_CMD_DRIVE_ENABLE);
    TEST_ASSERT_EQUAL_UINT8(3, HMI_CMD_RESET);
    TEST_ASSERT_EQUAL_UINT8(4, HMI_CMD_EMERGENCY_STOP);
    TEST_ASSERT_EQUAL_UINT8(5, HMI_CMD_RESERVED_ID);
    TEST_ASSERT_EQUAL_UINT8(6, HMI_CMD_STOP);
}

// E-STOP ile RESET'in KARISMADIGI — bu suite'in var olma sebebi.
void test_estop_is_not_reset(void) {
    TEST_ASSERT_NOT_EQUAL(HMI_CMD_RESET, HMI_CMD_EMERGENCY_STOP);
    TEST_ASSERT_EQUAL_UINT8(4, HMI_CMD_EMERGENCY_STOP);
    TEST_ASSERT_EQUAL_UINT8(3, HMI_CMD_RESET);
}

// ===========================================================================
// 2. Cerceve cozumu — her komut icin `0x5A <CMD> <~CMD>`
// ===========================================================================

void test_frame_start(void) {  // printh 5A 01 FE
    assertFrameDecodes(HMI_CMD_START);
}

void test_frame_drive_enable(void) {  // printh 5A 02 FD
    assertFrameDecodes(HMI_CMD_DRIVE_ENABLE);
}

void test_frame_reset(void) {  // printh 5A 03 FC
    assertFrameDecodes(HMI_CMD_RESET);
}

void test_frame_emergency_stop(void) {  // printh 5A 04 FB
    assertFrameDecodes(HMI_CMD_EMERGENCY_STOP);
}

// ID 5 (rezerve) icin cerceve YINE DE cozulur — parser komut-agnostiktir,
// yalnizca checksum'a bakar. Bu KASITLIDIR: cerceve main.cpp'ye komut 5 olarak
// ulasir ve orada `default` dalinda WARN'lanip yutulur. Yani "sessizce yok
// sayma" parser katmaninda DEGIL, switch katmanindadir.
void test_frame_reserved_id_still_parses_but_is_unassigned(void) {  // printh 5A 05 FA
    assertFrameDecodes(HMI_CMD_RESERVED_ID);

    // Rezerve ID hicbir gecerli komutla CAKISMAMALI — main.cpp'de kendine ait
    // bir `case` OLMADIGININ sartidir (aksi halde default dalina dusmezdi).
    TEST_ASSERT_NOT_EQUAL(HMI_CMD_START, HMI_CMD_RESERVED_ID);
    TEST_ASSERT_NOT_EQUAL(HMI_CMD_DRIVE_ENABLE, HMI_CMD_RESERVED_ID);
    TEST_ASSERT_NOT_EQUAL(HMI_CMD_RESET, HMI_CMD_RESERVED_ID);
    TEST_ASSERT_NOT_EQUAL(HMI_CMD_EMERGENCY_STOP, HMI_CMD_RESERVED_ID);
    TEST_ASSERT_NOT_EQUAL(HMI_CMD_STOP, HMI_CMD_RESERVED_ID);
}

void test_frame_stop(void) {  // printh 5A 06 F9
    assertFrameDecodes(HMI_CMD_STOP);
}

// ===========================================================================
// 3. Dokumandaki checksum tablosunun (§0.3) kod ile ayni oldugu
//
// NEXTION_EKRAN_YAPILACAKLAR.md §0.3 ekran tarafina "3. byte = 0xFF - CMD"
// diyor; parser ise `~CMD` kullaniyor. Ikisinin ayni oldugunu burada kilitle.
// ===========================================================================

void test_checksum_table_matches_document(void) {
    TEST_ASSERT_EQUAL_HEX8(0xFE, expectedChecksum(HMI_CMD_START));
    TEST_ASSERT_EQUAL_HEX8(0xFD, expectedChecksum(HMI_CMD_DRIVE_ENABLE));
    TEST_ASSERT_EQUAL_HEX8(0xFC, expectedChecksum(HMI_CMD_RESET));
    TEST_ASSERT_EQUAL_HEX8(0xFB, expectedChecksum(HMI_CMD_EMERGENCY_STOP));
    TEST_ASSERT_EQUAL_HEX8(0xFA, expectedChecksum(HMI_CMD_RESERVED_ID));
    TEST_ASSERT_EQUAL_HEX8(0xF9, expectedChecksum(HMI_CMD_STOP));

    // "0xFF - CMD" kestirmesi `~CMD` ile birebir ayni (CMD <= 0xFF oldugu icin).
    for (unsigned cmd = 0; cmd <= 0xFF; ++cmd) {
        TEST_ASSERT_EQUAL_HEX8((uint8_t)(0xFF - cmd),
                               expectedChecksum((uint8_t)cmd));
    }
}

// ===========================================================================
// 4. REGRESYON TANIGI — eski (yanlis) dokuman tablosunun sonucu
//
// Eski tablo E-STOP icin `0x5A 03 FC` diyordu. Bu test o cercevenin firmware'de
// EMERGENCY_STOP'a DEGIL RESET'e cozuldugunu kayda geciriyor: yanlis dokuman
// gercekten tehlikeliydi ve dogru cerceve `0x5A 04 FB`'dir.
// ===========================================================================

void test_old_doc_estop_frame_actually_decodes_to_reset(void) {
    HMI_TouchParserState state;
    uint8_t outCmd = 0;

    // Eski dokumanin "E-STOP" cercevesi: 5A 03 FC
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x5A, state, outCmd));
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x03, state, outCmd));
    TEST_ASSERT_TRUE(HMI_parseTouchByte(0xFC, state, outCmd));

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        HMI_CMD_RESET, outCmd,
        "Eski dokumanin E-STOP cercevesi RESET'e cozulur — dogrusu 5A 04 FB");
    TEST_ASSERT_NOT_EQUAL(HMI_CMD_EMERGENCY_STOP, outCmd);
}

// Dogru E-STOP cercevesi gercekten EMERGENCY_STOP'a cozuluyor.
void test_correct_estop_frame_decodes_to_emergency_stop(void) {
    HMI_TouchParserState state;
    uint8_t outCmd = 0;

    // printh 5A 04 FB
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x5A, state, outCmd));
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x04, state, outCmd));
    TEST_ASSERT_TRUE(HMI_parseTouchByte(0xFB, state, outCmd));

    TEST_ASSERT_EQUAL_UINT8(HMI_CMD_EMERGENCY_STOP, outCmd);
}

// ===========================================================================
// 5. Komsu komutun checksum'u KABUL EDILMEMELI
//
// Ekran tarafinda tek bir hex hanesi yanlis yazilirsa (ornegin
// `printh 5A 04 FC` — E-STOP komutu, RESET checksum'u) cerceve REDDEDILMELI;
// yanlis eyleme sessizce donusmemeli.
// ===========================================================================

void test_mismatched_checksum_rejects_frame(void) {
    HMI_TouchParserState state;
    uint8_t outCmd = 0;

    // 5A 04 FC — E-STOP komutu ama RESET'in checksum'u
    TEST_ASSERT_FALSE(HMI_parseTouchByte(0x5A, state, outCmd));
    TEST_ASSERT_FALSE(HMI_parseTouchByte(HMI_CMD_EMERGENCY_STOP, state, outCmd));
    TEST_ASSERT_FALSE_MESSAGE(
        HMI_parseTouchByte(expectedChecksum(HMI_CMD_RESET), state, outCmd),
        "Yanlis checksum'lu cerceve KABUL EDILMEMELI");

    // Parser durumu resetlenmis olmali: hemen ardindan gelen dogru cerceve
    // sorunsuz cozulmeli.
    assertFrameDecodes(HMI_CMD_EMERGENCY_STOP);
}

// ===========================================================================
// 6. Butun komutlar ARDI ARDINA, tek parser durumu uzerinden
//
// Gercek RX akisinda cerceveler pespese gelir. Her komut, bir oncekinden
// bagimsiz olarak dogru cozulmeli (durum sizintisi olmamali).
// ===========================================================================

void test_all_commands_decode_back_to_back(void) {
    const uint8_t commands[] = {
        HMI_CMD_START,           // 5A 01 FE
        HMI_CMD_DRIVE_ENABLE,    // 5A 02 FD
        HMI_CMD_RESET,           // 5A 03 FC
        HMI_CMD_EMERGENCY_STOP,  // 5A 04 FB
        HMI_CMD_RESERVED_ID,  // 5A 05 FA
        HMI_CMD_STOP,            // 5A 06 F9
    };

    HMI_TouchParserState state;

    for (uint8_t cmd : commands) {
        uint8_t outCmd = 0xAA;
        TEST_ASSERT_FALSE(HMI_parseTouchByte(HMI_FRAME_HEADER, state, outCmd));
        TEST_ASSERT_FALSE(HMI_parseTouchByte(cmd, state, outCmd));
        TEST_ASSERT_TRUE(
            HMI_parseTouchByte(expectedChecksum(cmd), state, outCmd));
        TEST_ASSERT_EQUAL_UINT8(cmd, outCmd);
    }
}
