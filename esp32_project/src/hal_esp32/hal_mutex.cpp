/**
 * @file hal_mutex.cpp
 * @brief HAL Mutex — FreeRTOS recursive mutex for NavigationState protection.
 *
 * Domain: Synchronization
 * Split from hal_impl.cpp (ADR-HAL-002).
 */

#include "hal/hal.h"
#include "logic/log_config.h"
#include "logic/log_ext.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_HAL
#include "esp_log.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ===================================================================
// State mutex (recursive, for NavigationState)
// ===================================================================
#if configSUPPORT_STATIC_ALLOCATION
static StaticSemaphore_t s_mutex_buffer;
#endif
static SemaphoreHandle_t s_mutex = nullptr;

// ===================================================================
// Serial log mutex — protects USB CDC Serial from concurrent access.
// Created here in hal_mutex_init(), used in hal_logging.cpp.
// ===================================================================
SemaphoreHandle_t s_log_mutex = nullptr;

// ===================================================================
// Public API (hal.h)
// ===================================================================

void hal_mutex_init(void) {
#if configSUPPORT_STATIC_ALLOCATION
    s_mutex = xSemaphoreCreateRecursiveMutexStatic(&s_mutex_buffer);
#else
    s_mutex = xSemaphoreCreateRecursiveMutex();
#endif

    // Serial log mutex (binary, protects USB CDC from concurrent access)
    s_log_mutex = xSemaphoreCreateMutex();
}

void hal_mutex_lock(void) {
    if (s_mutex) {
        xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    }
}

void hal_mutex_unlock(void) {
    if (s_mutex) {
        xSemaphoreGiveRecursive(s_mutex);
    }
}
