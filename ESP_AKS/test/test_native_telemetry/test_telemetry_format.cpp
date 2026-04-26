#include <unity.h>

#include <cstring>

#include "Telemetry.h"
#include "fake_uart.h"

namespace {

TelemetryData makeZeroData() {
    TelemetryData d{};
    return d;
}

TelemetryData makeDistinctData() {
    TelemetryData d{};
    d.TEL_motorRpm = 1500;
    d.TEL_motorTorqueFeedback = -250;
    d.TEL_motorErrorFlags = 5;
    d.TEL_motorDataValid = true;
    d.TEL_motorTimeoutActive = false;
    d.TEL_bmsSoc = 87;
    d.TEL_bmsCurrentDeciA = -90;
    d.TEL_bmsTemperatureC = 42;
    d.TEL_bmsPackVoltageDeciV = 78;
    d.TEL_bmsAverageCellVoltageMv = 3950;
    d.TEL_bmsErrorFlags = 0;
    d.TEL_bmsDataValid = true;
    return d;
}

}  // namespace

// ---------------------------------------------------------------------------
// begin() çağrılmadan sendStatus(): hiçbir byte UART'a yazılmamalı.
// ---------------------------------------------------------------------------
void test_no_write_before_begin(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.sendStatus(makeZeroData());
    TEST_ASSERT_EQUAL_size_t(0, fake_uart_get_size());
}

// ---------------------------------------------------------------------------
// İlk paket "TEL,1,0," ile başlamalı (versiyon=1, seq=0).
// ---------------------------------------------------------------------------
void test_first_packet_has_v1_seq0_prefix(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());

    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,1,0,"));
}

// ---------------------------------------------------------------------------
// Ardışık 3 çağrı: seq 0,1,2 olarak monoton artmalı.
// ---------------------------------------------------------------------------
void test_sequence_increments(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());
    tel.sendStatus(makeZeroData());
    tel.sendStatus(makeZeroData());

    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,1,0,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,1,1,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,1,2,"));
}

// ---------------------------------------------------------------------------
// begin() çağrısı sequence counter'ı 0'a sıfırlamalı.
// ---------------------------------------------------------------------------
void test_begin_resets_sequence(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());
    tel.sendStatus(makeZeroData());  // seq 0 ve 1

    fake_uart_reset();  // önceki içeriği temizle
    tel.begin();        // yeniden başlat
    tel.sendStatus(makeZeroData());

    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_NOT_NULL(strstr(buf, "TEL,1,0,"));
    // Hâlâ 1 ile başlayan yeni bir kayıt OLMAMALI (yalnız tek sendStatus geçti).
    TEST_ASSERT_NULL(strstr(buf, "TEL,1,1,"));
}

// ---------------------------------------------------------------------------
// Paket "\r\n" ile sonlanmalı.
// ---------------------------------------------------------------------------
void test_packet_ends_with_crlf(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());

    size_t sz = fake_uart_get_size();
    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_TRUE(sz >= 2);
    TEST_ASSERT_EQUAL_CHAR('\r', buf[sz - 2]);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[sz - 1]);
}

// ---------------------------------------------------------------------------
// Negatif torque işaretle birlikte yazılmalı.
// ---------------------------------------------------------------------------
void test_negative_torque_is_formatted(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_motorTorqueFeedback = -500;
    tel.sendStatus(d);

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",-500,"));
}

void test_negative_current_is_formatted(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_bmsCurrentDeciA = -128;
    tel.sendStatus(d);

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",-128,"));
}

void test_negative_temperature_is_formatted(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_bmsTemperatureC = -20;
    tel.sendStatus(d);

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), ",-20,"));
}

// ---------------------------------------------------------------------------
// Boolean alanlar 0/1 olarak render edilmeli.
// ---------------------------------------------------------------------------
void test_motor_valid_renders_as_one(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_motorDataValid = true;
    tel.sendStatus(d);

    // Format: TEL,1,0,rpm,torque,motorErr,motorValid,motorTimeout,...
    //                  ↓        ↓           ↓
    //   "TEL,1,0,0,0,0,1,0,..."
    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), "TEL,1,0,0,0,0,1,0,"));
}

void test_motor_timeout_renders_as_one(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_motorTimeoutActive = true;
    tel.sendStatus(d);

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), "TEL,1,0,0,0,0,0,1,"));
}

void test_bms_valid_renders_as_one(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    TelemetryData d = makeZeroData();
    d.TEL_bmsDataValid = true;
    tel.sendStatus(d);

    // BMS valid son alandır → ",1\r\n" ile bitmeli.
    size_t sz = fake_uart_get_size();
    const char* buf = fake_uart_get_buffer();
    TEST_ASSERT_TRUE(sz >= 4);
    TEST_ASSERT_EQUAL_CHAR(',', buf[sz - 4]);
    TEST_ASSERT_EQUAL_CHAR('1', buf[sz - 3]);
    TEST_ASSERT_EQUAL_CHAR('\r', buf[sz - 2]);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[sz - 1]);
}

// ---------------------------------------------------------------------------
// Tüm alanları farklı değerlerle dolduran tam payload kontrolü.
// ---------------------------------------------------------------------------
void test_full_format_with_distinct_values(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeDistinctData());

    const char* buf = fake_uart_get_buffer();
    const char* expected =
        "TEL,1,0,1500,-250,5,1,0,87,-90,42,78,3950,0,1\r\n";
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

// ---------------------------------------------------------------------------
// İki paket aralarında "\r\nTEL," ayırıcısı olmalı.
// ---------------------------------------------------------------------------
void test_two_packets_have_separator(void) {
    fake_uart_reset();
    Telemetry tel;
    tel.begin();
    tel.sendStatus(makeZeroData());
    tel.sendStatus(makeZeroData());

    TEST_ASSERT_NOT_NULL(strstr(fake_uart_get_buffer(), "\r\nTEL,"));
}
