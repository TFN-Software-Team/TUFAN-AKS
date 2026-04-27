#include "fake_spi.h"

#include <cstdint>
#include <vector>

#include "driver/spi_master.h"
#include "esp_err.h"

namespace {
std::vector<FakeSpiWrite> s_writes;
int s_dummy_device = 1;  // non-null sentinel for spi_device_handle_t
}  // namespace

extern "C" {

esp_err_t spi_bus_initialize(spi_host_device_t /*host*/,
                             const spi_bus_config_t* /*cfg*/,
                             int /*dma*/) {
    return ESP_OK;
}

esp_err_t spi_bus_add_device(spi_host_device_t /*host*/,
                             const spi_device_interface_config_t* /*cfg*/,
                             spi_device_handle_t* handle) {
    if (handle != nullptr)
        *handle = &s_dummy_device;
    return ESP_OK;
}

esp_err_t spi_device_transmit(spi_device_handle_t /*h*/,
                              spi_transaction_t* trans) {
    if (trans != nullptr && trans->tx_buffer != nullptr) {
        const uint8_t* p = static_cast<const uint8_t*>(trans->tx_buffer);
        // RelayManager her zaman 3 byte yazar: [ADDR, REG, VALUE]
        FakeSpiWrite w;
        w.reg = p[1];
        w.value = p[2];
        s_writes.push_back(w);
    }
    return ESP_OK;
}

const char* esp_err_to_name(esp_err_t /*err*/) {
    return "FAKE_ERR";
}

}  // extern "C"

size_t fake_spi_write_count(void) {
    return s_writes.size();
}

FakeSpiWrite fake_spi_write_at(size_t i) {
    if (i >= s_writes.size())
        return {0, 0};
    return s_writes[i];
}

FakeSpiWrite fake_spi_last_write(void) {
    if (s_writes.empty())
        return {0, 0};
    return s_writes.back();
}

void fake_spi_reset(void) {
    s_writes.clear();
}
