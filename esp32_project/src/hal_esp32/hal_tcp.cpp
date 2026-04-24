/**
 * @file hal_tcp.cpp
 * @brief TCP Client for NTRIP over Ethernet (WiFiClient on ESP-IDF ETH).
 *
 * Domain: TCP/IP Client
 * Split from hal_impl.cpp (ADR-HAL-002).
 *
 * Uses WiFiClient which works with the ESP-IDF ETH network interface.
 * W5500 has max 8 sockets; NTRIP TCP uses one additional socket.
 */

#include "hal/hal.h"
#include "logic/log_config.h"
#include "logic/log_ext.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_HAL
#include "esp_log.h"

#include <Arduino.h>
#include <WiFi.h>

// ===================================================================
// TCP Client state
// ===================================================================
static WiFiClient s_tcp_client;
static bool s_tcp_connected = false;

// ===================================================================
// Public API (hal.h)
// ===================================================================

bool hal_tcp_connect(const char* host, uint16_t port) {
    if (!host || host[0] == '\0') {
        LOGE("HAL-TCP", "connect failed: host is null or empty");
        return false;
    }

    LOGI("HAL-TCP", "connecting to %s:%u", host, static_cast<unsigned>(port));
    s_tcp_client.setTimeout(3000);  // 3s connect timeout (WDT-safe, IDLE task needs to run)
    const bool ok = s_tcp_client.connect(host, port);
    if (ok) {
        s_tcp_connected = true;
        LOGI("HAL-TCP", "connected to %s:%u", host, static_cast<unsigned>(port));
    } else {
        s_tcp_connected = false;
        LOGW("HAL-TCP", "connect to %s:%u failed", host, static_cast<unsigned>(port));
    }
    return ok;
}

size_t hal_tcp_write(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;
    if (!s_tcp_connected || !s_tcp_client.connected()) {
        s_tcp_connected = false;
        return 0;
    }
    return s_tcp_client.write(data, len);
}

int hal_tcp_read(uint8_t* buf, size_t max_len) {
    if (!buf || max_len == 0) return 0;
    if (!s_tcp_connected || !s_tcp_client.connected()) {
        s_tcp_connected = false;
        return 0;
    }
    return s_tcp_client.read(buf, max_len);
}

int hal_tcp_available(void) {
    if (!s_tcp_connected || !s_tcp_client.connected()) {
        s_tcp_connected = false;
        return -1;
    }
    return s_tcp_client.available();
}

bool hal_tcp_connected(void) {
    if (!s_tcp_connected) return false;
    if (!s_tcp_client.connected()) {
        s_tcp_connected = false;
        return false;
    }
    return true;
}

void hal_tcp_disconnect(void) {
    s_tcp_client.stop();
    s_tcp_connected = false;
}
