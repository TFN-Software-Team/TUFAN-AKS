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
        .clock_speed_hz = 1000000,  // 1 MHz — MCP23S17 max 10MHz, conservative
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

    // Set all pins as output
    writeRegister(MCP23S17_IODIRA, 0x00);
    writeRegister(MCP23S17_IODIRB, 0x00);

    allOff();
    s_initialized = true;
    ESP_LOGI(TAG, "RelayManager initialized — all relays OFF");
    return true;
}

void RelayManager::setRelay(uint8_t channel, bool state) {
    if (!s_initialized || channel > 9)
        return;

    if (state)
        s_relayState |= (1 << channel);
    else
        s_relayState &= ~(1 << channel);

    // OUT0-OUT7 → PORTA, OUT8-OUT9 → PORTB
    writeRegister(MCP23S17_OLATA, s_relayState & 0xFF);
    writeRegister(MCP23S17_OLATB, (s_relayState >> 8) & 0xFF);

    ESP_LOGD(TAG, "Relay %d → %s", channel, state ? "ON" : "OFF");
}

void RelayManager::allOff() {
    s_relayState = 0;
    writeRegister(MCP23S17_OLATA, 0x00);
    writeRegister(MCP23S17_OLATB, 0x00);
    ESP_LOGW(TAG, "All relays de-energized");
}

void RelayManager::writeRegister(uint8_t reg, uint8_t value) {
    uint8_t tx[3] = {MCP23S17_ADDR, reg, value};
    spi_transaction_t t = {};
    t.length = 24;  // 3 bytes
    t.tx_buffer = tx;
    spi_device_transmit(s_spiDev, &t);
}