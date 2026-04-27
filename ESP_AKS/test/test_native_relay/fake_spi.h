#pragma once
// Native testler için SPI yakalayıcı.
// spi_device_transmit her çağrıda 3 byte alır: [MCP23S17_ADDR, register, value].
// Bu fake yalnızca (register, value) çiftini biriktirir.
#include <cstddef>
#include <cstdint>

struct FakeSpiWrite {
    uint8_t reg;
    uint8_t value;
};

size_t       fake_spi_write_count(void);
FakeSpiWrite fake_spi_write_at(size_t i);
FakeSpiWrite fake_spi_last_write(void);
void         fake_spi_reset(void);
