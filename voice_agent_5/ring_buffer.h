#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "esp32-hal-psram.h"

// =====================================================================
// PSRAM THREAD-SAFE RING BUFFER
// =====================================================================
class PSRAMRingBuffer {
private:
    uint8_t* buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;
    SemaphoreHandle_t mutex;
public:
    PSRAMRingBuffer(size_t capacity) {
        size = capacity;
        buffer = (uint8_t*)ps_malloc(size);
        head = 0; tail = 0; count = 0;
        mutex = xSemaphoreCreateMutex();
    }
    bool write(const uint8_t* data, size_t len) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        if (size - count < len) {
            xSemaphoreGive(mutex);
            return false;
        }
        for (size_t i = 0; i < len; i++) {
            buffer[head] = data[i];
            head = (head + 1) % size;
        }
        count += len;
        xSemaphoreGive(mutex);
        return true;
    }
    size_t read(uint8_t* data, size_t len) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        size_t available = (count < len) ? count : len;
        for (size_t i = 0; i < available; i++) {
            data[i] = buffer[tail];
            tail = (tail + 1) % size;
        }
        count -= available;
        xSemaphoreGive(mutex);
        return available;
    }
    size_t available() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        size_t c = count;
        xSemaphoreGive(mutex);
        return c;
    }
    bool isValid() const { return buffer != nullptr && mutex != nullptr; }
    void clear() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        head = 0; tail = 0; count = 0;
        xSemaphoreGive(mutex);
    }
};
