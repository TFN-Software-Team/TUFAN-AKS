#pragma once
// Native test stub for driver/spi_master.h
// RelayManager.cpp'yi native ortamda derlemek için gereken minimal SPI tip
// ve fonksiyon imzaları. Gerçek davranış fake_spi.cpp tarafından sağlanır.
#include <cstddef>
#include <cstdint>

#include "esp_err.h"

typedef void* spi_device_handle_t;
typedef int   spi_host_device_t;

#define SPI2_HOST       ((spi_host_device_t)1)
#define SPI_DMA_CH_AUTO 3

typedef struct {
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
} spi_bus_config_t;

typedef struct {
    uint8_t  mode;
    int      clock_speed_hz;
    int      spics_io_num;
    int      queue_size;
    uint16_t flags;
    int      input_delay_ns;
    int      sample_point;
    void*    pre_cb;
    void*    post_cb;
} spi_device_interface_config_t;

typedef struct {
    uint32_t    length;        // bits
    uint32_t    rxlength;
    void*       user;
    const void* tx_buffer;
    void*       rx_buffer;
} spi_transaction_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t spi_bus_initialize(spi_host_device_t host,
                             const spi_bus_config_t* cfg, int dma_chan);
esp_err_t spi_bus_add_device(spi_host_device_t host,
                             const spi_device_interface_config_t* cfg,
                             spi_device_handle_t* handle);
esp_err_t spi_device_transmit(spi_device_handle_t handle,
                              spi_transaction_t* trans);

#ifdef __cplusplus
}
#endif
