#pragma once
#include "FreeRTOS.h"

typedef void* QueueHandle_t;

#ifdef __cplusplus
extern "C" {
#endif

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
BaseType_t    xQueueSend(QueueHandle_t q, const void* item, TickType_t ticksToWait);
BaseType_t    xQueueReceive(QueueHandle_t q, void* dst, TickType_t ticksToWait);

#ifdef __cplusplus
}
#endif
