#pragma once
// Native test stub for driver/twai.h
// Yalnız parsing testleri için gereken sembolleri sağlar; gerçek TWAI driver
// API'si (twai_driver_install, twai_transmit, vs.) burada YOK — çünkü bu
// fonksiyonları kullanan CanManager.cpp native build'de zaten lib_ignore ile
// dışlanır.
#include <cstdint>

#define TWAI_FRAME_MAX_DLC 8

typedef struct {
    uint32_t identifier;
    uint8_t  data_length_code;
    uint8_t  data[TWAI_FRAME_MAX_DLC];
    uint32_t flags;
} twai_message_t;
