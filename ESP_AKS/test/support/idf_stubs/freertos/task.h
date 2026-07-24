#pragma once
// Native test stub for freertos/task.h — şu an Faz 2 için boş; ilerleyen
// fazlarda xTaskGetTickCount vb. eklenecek.
#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t xTicksToDelay);

#ifdef __cplusplus
}
#endif
