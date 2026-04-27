#include "RelayManager.h"
#include "SystemConfig.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static constexpr const char* TAG = "RelayManager";

RelayManager& RelayManager::instance() {
    static RelayManager inst;
    return inst;
}

bool RelayManager::begin() {
    spi_bus_config_t buscfg = {
        .mosi_io_num = RELAY_SPI_MOSI,
        .miso_io_num = RELAY_SPI_MISO,
        .sclk_io_num = RELAY_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 1000000,
        .spics_io_num = RELAY_SPI_CS,
        .queue_size = 1,
    };

    esp_err_t ret =
        spi_bus_initialize(RELAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = spi_bus_add_device(RELAY_SPI_HOST, &devcfg, &s_spiDev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return false;
    }

    // CRITICAL: Write OLAT registers BEFORE setting pins as output.
    // MCP23S17 defaults OLAT to 0x00 (LOW).  In our active-low hardware
    // LOW = relay ON, so we must set 0xFF (HIGH = relay OFF) first to
    // prevent all relays firing the instant IODIR switches to output.
    writeRegister(MCP23S17_OLATA, 0xFF);
    writeRegister(MCP23S17_OLATB, 0xFF);

    // Now safe to set all pins as output
    writeRegister(MCP23S17_IODIRA, 0x00);
    writeRegister(MCP23S17_IODIRB, 0x00);

    s_relayState = 0;
    s_initialized = true;
    ESP_LOGI(TAG, "RelayManager initialized — all relays OFF");
    return true;
}

void RelayManager::setRelay(uint8_t channel, bool state) {
    if (!s_initialized || channel >= RELAY_TOTAL_CHANNELS) {
        ESP_LOGW(TAG, "setRelay: invalid call (init=%d, ch=%d)", s_initialized,
                 channel);
        return;
    }

    if (state)
        s_relayState |= (1u << channel);
    else
        s_relayState &= ~(1u << channel);

    // Hardware is active-low: invert logical state before writing
    uint16_t hw = ~s_relayState;
    writeRegister(MCP23S17_OLATA, hw & 0xFF);
    writeRegister(MCP23S17_OLATB, (hw >> 8) & 0xFF);

    ESP_LOGD(TAG, "Relay %d → %s (state=0x%03X)", channel, state ? "ON" : "OFF",
             s_relayState);
}

void RelayManager::allOn() {
    if (!s_initialized) {
        ESP_LOGW(TAG, "allOn called before begin()");
        return;
    }
    s_relayState = (1u << RELAY_TOTAL_CHANNELS) - 1;  // bits 0-9 set
    // Active-low: all relays ON = all used pins LOW
    uint16_t hw = ~s_relayState;
    writeRegister(MCP23S17_OLATA, hw & 0xFF);
    writeRegister(MCP23S17_OLATB, (hw >> 8) & 0xFF);
    ESP_LOGI(TAG, "All %d contactors closed", RELAY_TOTAL_CHANNELS);
}

void RelayManager::allOff() {
    s_relayState = 0;
    // Active-low: all relays OFF = all pins HIGH
    writeRegister(MCP23S17_OLATA, 0xFF);
    writeRegister(MCP23S17_OLATB, 0xFF);
    ESP_LOGW(TAG, "All relays de-energized");
}

bool RelayManager::getRelayState(uint8_t channel) const {
    if (channel >= RELAY_TOTAL_CHANNELS)
        return false;
    return (s_relayState >> channel) & 0x01;
}

#ifdef NATIVE_BUILD
void RelayManager::resetForTest() {
    s_relayState = 0;
    s_initialized = false;
    s_spiDev = nullptr;
}
#endif

void RelayManager::writeRegister(uint8_t reg, uint8_t value) {
    if (s_spiDev == nullptr)
        return;

    uint8_t tx[3] = {MCP23S17_ADDR, reg, value};
    spi_transaction_t t = {};
    t.length = 24;
    t.tx_buffer = tx;

    esp_err_t ret = spi_device_transmit(s_spiDev, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write failed: reg=0x%02X val=0x%02X err=%s", reg,
                 value, esp_err_to_name(ret));
    }
}