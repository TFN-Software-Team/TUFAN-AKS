#pragma once
// Native test stub for freertos/FreeRTOS.h
// Sadece tip ve sabitleri sağlar; FreeRTOS scheduler emülasyonu yapmaz.
#include <cstdint>

typedef int       BaseType_t;
typedef unsigned  UBaseType_t;
typedef uint32_t  TickType_t;

#define pdTRUE          ((BaseType_t)1)
#define pdFALSE         ((BaseType_t)0)
#define pdPASS          pdTRUE
#define pdFAIL          pdFALSE
#define portMAX_DELAY   ((TickType_t)0xFFFFFFFFu)
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
