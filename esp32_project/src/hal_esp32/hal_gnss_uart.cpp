/**
 * @file hal_gnss_uart.cpp
 * @brief GNSS UART — RTCM correction stream and multi-receiver indexed UART.
 *
 * Domain: GNSS UART Communication
 * Split from hal_impl.cpp (ADR-HAL-002).
 *
 * - hal_gnss_rtcm_* functions for the primary RTCM correction stream
 * - hal_gnss_uart_* functions for indexed multi-receiver support (TASK-025)
 */

#include "hal/hal.h"
#include "hal_esp32_internal.h"
#include "hal_impl.h"
#include "fw_config.h"
#include "logic/log_config.h"
#include "logic/log_ext.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_HAL
#include "esp_log.h"

#include <Arduino.h>
#include <cstring>

// ===================================================================
// GNSS RTCM UART (UM980 corrections, 8N1)
// ===================================================================
static HardwareSerial* s_gnss_rtcm_uart = &Serial1;
static uint8_t s_gnss_rtcm_uart_num = 1;
static SemaphoreHandle_t s_gnss_rtcm_mutex = nullptr;
static bool s_gnss_rtcm_ready = false;
static uint32_t s_gnss_rtcm_drop_bytes = 0;

static HardwareSerial* gnssUartForNum(uint8_t uart_num) {
    switch (uart_num) {
    case 1: return &Serial1;
    case 2: return &Serial2;
    default: return nullptr;
    }
}

// ===================================================================
// Indexed GNSS UART (multi-receiver) — TASK-025
// ===================================================================
static HardwareSerial* s_gnss_uart_inst[GNSS_RX_MAX] = {};
static bool   s_gnss_uart_inst_ready[GNSS_RX_MAX] = {};
static uint32_t s_gnss_uart_inst_drop[GNSS_RX_MAX] = {};
static SemaphoreHandle_t s_gnss_uart_inst_mutex[GNSS_RX_MAX] = {};

static HardwareSerial* gnssUartForInst(uint8_t inst) {
    if (inst == 0) return s_gnss_rtcm_uart;
    if (inst >= GNSS_RX_MAX) return nullptr;
    return s_gnss_uart_inst[inst];
}

// ===================================================================
// Public API — GNSS RTCM UART (hal.h, hal_impl.h)
// ===================================================================

void hal_esp32_gnss_rtcm_set_uart(uint8_t uart_num) {
    HardwareSerial* uart = gnssUartForNum(uart_num);
    if (!uart) {
        LOGW("HAL", "GNSS RTCM UART%u unsupported (valid: 1 or 2), keeping UART%u",
             static_cast<unsigned>(uart_num),
             static_cast<unsigned>(s_gnss_rtcm_uart_num));
        return;
    }
    if (s_gnss_rtcm_ready) {
        LOGW("HAL", "GNSS RTCM UART already active on UART%u, switch ignored",
             static_cast<unsigned>(s_gnss_rtcm_uart_num));
        return;
    }
    s_gnss_rtcm_uart_num = uart_num;
    s_gnss_rtcm_uart = uart;
    LOGI("HAL", "GNSS RTCM mapped to UART%u", static_cast<unsigned>(s_gnss_rtcm_uart_num));
}

bool hal_gnss_rtcm_begin(uint32_t baud, int8_t rx_pin, int8_t tx_pin) {
    if (!s_gnss_rtcm_uart) {
        LOGE("HAL", "GNSS RTCM begin failed: UART mapping is null");
        return false;
    }
    if (baud == 0) {
        LOGE("HAL", "GNSS RTCM begin failed: baud must be > 0");
        return false;
    }
    if (tx_pin < 0) {
        LOGE("HAL", "GNSS RTCM begin failed: TX pin must be >= 0");
        return false;
    }
#if defined(CONFIG_IDF_TARGET_ESP32)
    if (tx_pin >= 34 && tx_pin <= 39) {
        LOGE("HAL", "GNSS RTCM begin failed: TX pin %d is input-only on ESP32", tx_pin);
        return false;
    }
#endif

    if (!s_gnss_rtcm_mutex) {
        s_gnss_rtcm_mutex = xSemaphoreCreateMutex();
    }
    if (s_gnss_rtcm_mutex) {
        xSemaphoreTake(s_gnss_rtcm_mutex, portMAX_DELAY);
    }

    HalGnssUartPins defaults = hal_esp32_gnss_uart_pins_for_num(s_gnss_rtcm_uart_num);
    const int uart_rx = (rx_pin < 0) ? static_cast<int>(defaults.rx) : static_cast<int>(rx_pin);
    const int uart_tx = (tx_pin < 0) ? static_cast<int>(defaults.tx) : static_cast<int>(tx_pin);

    if (uart_tx < 0) {
        LOGE("HAL", "GNSS RTCM begin failed: UART%u TX pin unresolved",
             static_cast<unsigned>(s_gnss_rtcm_uart_num));
        if (s_gnss_rtcm_mutex) {
            xSemaphoreGive(s_gnss_rtcm_mutex);
        }
        return false;
    }
    if (uart_rx >= 38 && uart_rx <= 42) {
        LOGE("HAL", "GNSS RTCM begin failed: RX pin %d is output-only on ESP32-S3", uart_rx);
        if (s_gnss_rtcm_mutex) {
            xSemaphoreGive(s_gnss_rtcm_mutex);
        }
        return false;
    }

    s_gnss_rtcm_uart->begin(baud, SERIAL_8N1, uart_rx, uart_tx);
    s_gnss_rtcm_ready = true;
    if (s_gnss_rtcm_mutex) {
        xSemaphoreGive(s_gnss_rtcm_mutex);
    }

    LOGI("HAL", "GNSS RTCM UART%u ready (baud=%lu, mode=8N1, rx=%d, tx=%d)",
         static_cast<unsigned>(s_gnss_rtcm_uart_num),
         static_cast<unsigned long>(baud),
         uart_rx,
         uart_tx);
    return true;
}

size_t hal_gnss_rtcm_write(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;
    if (!s_gnss_rtcm_mutex) {
        s_gnss_rtcm_mutex = xSemaphoreCreateMutex();
    }
    if (s_gnss_rtcm_mutex) {
        xSemaphoreTake(s_gnss_rtcm_mutex, portMAX_DELAY);
    }
    if (!s_gnss_rtcm_ready || !s_gnss_rtcm_uart) {
        LOGW("HAL", "GNSS RTCM write dropped (%u bytes): UART not initialised",
             static_cast<unsigned>(len));
        s_gnss_rtcm_drop_bytes += static_cast<uint32_t>(len);
        if (s_gnss_rtcm_mutex) {
            xSemaphoreGive(s_gnss_rtcm_mutex);
        }
        return 0;
    }

    const size_t written = s_gnss_rtcm_uart->write(data, len);
    if (s_gnss_rtcm_mutex) {
        xSemaphoreGive(s_gnss_rtcm_mutex);
    }

    if (written < len) {
        const size_t dropped = len - written;
        s_gnss_rtcm_drop_bytes += static_cast<uint32_t>(dropped);
        LOGW("HAL", "GNSS RTCM short write: %u/%u bytes", static_cast<unsigned>(written), static_cast<unsigned>(len));
    }

    return written;
}

bool hal_gnss_rtcm_is_ready(void) {
    return s_gnss_rtcm_ready;
}

uint32_t hal_gnss_rtcm_drop_count(void) {
    if (!s_gnss_rtcm_mutex) {
        return s_gnss_rtcm_drop_bytes;
    }
    xSemaphoreTake(s_gnss_rtcm_mutex, portMAX_DELAY);
    const uint32_t dropped = s_gnss_rtcm_drop_bytes;
    xSemaphoreGive(s_gnss_rtcm_mutex);
    return dropped;
}

// ===================================================================
// Public API — Indexed GNSS UART (hal.h)
// ===================================================================

bool hal_gnss_uart_begin(uint8_t inst, uint32_t baud, int8_t rx_pin, int8_t tx_pin) {
    if (inst >= GNSS_RX_MAX) {
        LOGE("HAL", "GNSS UART begin: inst %u out of range (max=%u)",
             static_cast<unsigned>(inst), static_cast<unsigned>(GNSS_RX_MAX));
        return false;
    }

    // inst=0: delegate to legacy API for full backward compatibility.
    if (inst == 0) {
        return hal_gnss_rtcm_begin(baud, rx_pin, tx_pin);
    }

    // inst>=1: use new per-instance path.
    if (baud == 0) {
        LOGE("HAL", "GNSS UART begin: inst %u baud must be > 0", static_cast<unsigned>(inst));
        return false;
    }
    if (tx_pin < 0) {
        LOGE("HAL", "GNSS UART begin: inst %u TX pin must be >= 0", static_cast<unsigned>(inst));
        return false;
    }
#if defined(CONFIG_IDF_TARGET_ESP32)
    if (tx_pin >= 34 && tx_pin <= 39) {
        LOGE("HAL", "GNSS UART begin: inst %u TX pin %d is input-only on ESP32",
             static_cast<unsigned>(inst), tx_pin);
        return false;
    }
#endif
    if (rx_pin >= 38 && rx_pin <= 42) {
        LOGE("HAL", "GNSS UART begin: inst %u RX pin %d is output-only on ESP32-S3",
             static_cast<unsigned>(inst), rx_pin);
        return false;
    }

    HardwareSerial* uart = nullptr;
    uint8_t uart_num = 0;
    if (inst == 1) {
        uart = &Serial2;
        uart_num = 2;
    } else {
        LOGE("HAL", "GNSS UART begin: inst %u has no UART mapping", static_cast<unsigned>(inst));
        return false;
    }

    if (!s_gnss_uart_inst_mutex[inst]) {
        s_gnss_uart_inst_mutex[inst] = xSemaphoreCreateMutex();
    }
    if (s_gnss_uart_inst_mutex[inst]) {
        xSemaphoreTake(s_gnss_uart_inst_mutex[inst], portMAX_DELAY);
    }

    if (!hal_esp32_claim_gnss_uart_pins(uart_num, rx_pin, tx_pin)) {
        if (s_gnss_uart_inst_mutex[inst]) {
            xSemaphoreGive(s_gnss_uart_inst_mutex[inst]);
        }
        return false;
    }

    uart->begin(baud, SERIAL_8N1, rx_pin, tx_pin);
    s_gnss_uart_inst[inst] = uart;
    s_gnss_uart_inst_ready[inst] = true;
    s_gnss_uart_inst_drop[inst] = 0;

    if (s_gnss_uart_inst_mutex[inst]) {
        xSemaphoreGive(s_gnss_uart_inst_mutex[inst]);
    }

    LOGI("HAL", "GNSS UART inst%u ready (UART%u, baud=%lu, rx=%d, tx=%d)",
         static_cast<unsigned>(inst), static_cast<unsigned>(uart_num),
         static_cast<unsigned long>(baud), rx_pin, tx_pin);
    return true;
}

size_t hal_gnss_uart_write(uint8_t inst, const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;

    // inst=0: delegate to legacy API.
    if (inst == 0) {
        return hal_gnss_rtcm_write(data, len);
    }

    if (inst >= GNSS_RX_MAX) return 0;

    if (!s_gnss_uart_inst_mutex[inst]) {
        s_gnss_uart_inst_mutex[inst] = xSemaphoreCreateMutex();
    }
    if (s_gnss_uart_inst_mutex[inst]) {
        xSemaphoreTake(s_gnss_uart_inst_mutex[inst], portMAX_DELAY);
    }

    if (!s_gnss_uart_inst_ready[inst] || !s_gnss_uart_inst[inst]) {
        s_gnss_uart_inst_drop[inst] += static_cast<uint32_t>(len);
        if (s_gnss_uart_inst_mutex[inst]) {
            xSemaphoreGive(s_gnss_uart_inst_mutex[inst]);
        }
        return 0;
    }

    const size_t written = s_gnss_uart_inst[inst]->write(data, len);
    if (s_gnss_uart_inst_mutex[inst]) {
        xSemaphoreGive(s_gnss_uart_inst_mutex[inst]);
    }

    if (written < len) {
        s_gnss_uart_inst_drop[inst] += static_cast<uint32_t>(len - written);
    }
    return written;
}

bool hal_gnss_uart_is_ready(uint8_t inst) {
    if (inst == 0) return s_gnss_rtcm_ready;
    if (inst >= GNSS_RX_MAX) return false;
    return s_gnss_uart_inst_ready[inst];
}
