#pragma once
#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;

#ifdef __cplusplus
extern "C" {
#endif

SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t        xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticksToWait);
BaseType_t        xSemaphoreGive(SemaphoreHandle_t s);

#ifdef __cplusplus
}
#endif
