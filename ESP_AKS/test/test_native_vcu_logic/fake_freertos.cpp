// Native testler için minimal FreeRTOS taklidi.
// Tek bir global FIFO ve no-op mutex sağlar — single-threaded test ortamında
// VcuLogic'in event queue ve mutex çağrılarını karşılar.
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

namespace {
std::deque<std::vector<uint8_t>> s_queue;
UBaseType_t s_itemSize = 0;
UBaseType_t s_capacity = 0;
int s_queueToken = 1;
int s_mutexToken = 1;
}  // namespace

extern "C" {

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
    s_queue.clear();
    s_itemSize = uxItemSize;
    s_capacity = uxQueueLength;
    return reinterpret_cast<QueueHandle_t>(&s_queueToken);
}

BaseType_t xQueueSend(QueueHandle_t /*q*/, const void* item,
                      TickType_t /*ticksToWait*/) {
    if (s_queue.size() >= s_capacity)
        return pdFALSE;
    std::vector<uint8_t> buf(s_itemSize);
    std::memcpy(buf.data(), item, s_itemSize);
    s_queue.push_back(std::move(buf));
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t /*q*/, void* dst,
                         TickType_t /*ticksToWait*/) {
    if (s_queue.empty())
        return pdFALSE;
    std::memcpy(dst, s_queue.front().data(), s_itemSize);
    s_queue.pop_front();
    return pdTRUE;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return reinterpret_cast<SemaphoreHandle_t>(&s_mutexToken);
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t /*s*/, TickType_t /*ticks*/) {
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t /*s*/) { return pdTRUE; }

}  // extern "C"

// Test helper — testler arası queue durumunu temizler.
void fake_freertos_reset(void) {
    s_queue.clear();
}
