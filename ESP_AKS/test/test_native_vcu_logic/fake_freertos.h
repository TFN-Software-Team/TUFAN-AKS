#pragma once
#include <cstdint>
// Fake FreeRTOS için reset helper'ı. setUp() içinde queue'yu temizlemek için.
#ifdef __cplusplus
extern "C" {
#endif
void fake_freertos_reset(void);
void fake_freertos_advance_time(uint32_t ms);
#ifdef __cplusplus
}
#endif
