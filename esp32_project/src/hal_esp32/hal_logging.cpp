/**
 * @file hal_logging.cpp
 * @brief HAL Logging — USB CDC Serial output with tag parsing and mutex protection.
 *
 * Domain: Logging
 * Split from hal_impl.cpp (ADR-HAL-002).
 *
 * hal_log() prints to USB CDC Serial via Serial.printf.
 * ESP_LOGI goes to UART0 by default, which does NOT appear on
 * USB CDC Serial on ESP32-S3. Serial.printf goes to USB CDC.
 *
 * hal_log() is kept for ABI compatibility with logic/ modules.
 * New code should use LOGI/LOGD/LOGW/LOGE from log_ext.h directly.
 *
 * The log mutex (s_log_mutex) is created in hal_mutex.cpp's hal_mutex_init()
 * and shared via external linkage.
 */

#include "hal/hal.h"
#include "debug/DebugConsole.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

// ===================================================================
// Serial log mutex — protects USB CDC Serial from concurrent access.
// USB CDC (Serial on ESP32-S3) is NOT thread-safe and will crash
// if two tasks call Serial.printf() simultaneously on different cores.
// ===================================================================
// s_log_mutex is created in hal_mutex.cpp (hal_mutex_init).
extern SemaphoreHandle_t s_log_mutex;

// ===================================================================
// Public API (hal.h)
// ===================================================================

void hal_log(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char category[20] = "LOG";
    char detail[20] = "";
    const char* body = buf;
    const char* colon = std::strchr(buf, ':');
    if (colon && colon != buf && (colon - buf) < 28) {
        bool tag_ok = true;
        for (const char* p = buf; p < colon; ++p) {
            const char c = *p;
            const bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            const bool digit = (c >= '0' && c <= '9');
            if (!alpha && !digit && c != '_' && c != '-') {
                tag_ok = false;
                break;
            }
        }

        if (tag_ok) {
            const char* dash = nullptr;
            for (const char* p = buf; p < colon; ++p) {
                if (*p == '-') {
                    dash = p;
                    break;
                }
            }

            const char* category_end = dash ? dash : colon;
            size_t category_len = static_cast<size_t>(category_end - buf);
            if (category_len >= sizeof(category)) category_len = sizeof(category) - 1;
            for (size_t i = 0; i < category_len; ++i) {
                const char c = buf[i];
                category[i] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
            }
            category[category_len] = '\0';

            if (dash && dash + 1 < colon) {
                size_t detail_len = static_cast<size_t>(colon - dash - 1);
                if (detail_len >= sizeof(detail)) detail_len = sizeof(detail) - 1;
                for (size_t i = 0; i < detail_len; ++i) {
                    const char c = dash[1 + i];
                    detail[i] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
                }
                detail[detail_len] = '\0';
            }

            body = colon + 1;
            while (*body == ' ') {
                body++;
            }
        }
    }

    // Protect output from concurrent access across cores.
    // DBG.write() fans out to Serial + TCP; the mutex protects both.
    if (s_log_mutex) {
        xSemaphoreTake(s_log_mutex, portMAX_DELAY);
    }
    if (detail[0] != '\0') {
        DBG.printf("[%s] [%10lu] %s: %s\n", category, millis(), detail, body);
    } else {
        DBG.printf("[%s] [%10lu] %s\n", category, millis(), body);
    }
    if (s_log_mutex) {
        xSemaphoreGive(s_log_mutex);
    }
}
