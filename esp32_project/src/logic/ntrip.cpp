/**
 * @file ntrip.cpp
 * @brief NTRIP client implementation — TASK-025.
 *
 * Implements the NTRIP protocol (Rev 2.0) for Single-Base casters:
 *   - TCP connection to caster with HTTP/1.0 request
 *   - Base64 Basic Authentication
 *   - RTCM data stream reading
 *   - Automatic reconnect on connection loss
 *
 * Data flow:
 *   ntripReadRtcm()  : TCP -> rtcm_buf (Input phase, commTask)
 *   ntripTick()      : State machine (task_slow — TASK-029)
 *   ntripForwardRtcm(): rtcm_buf -> UART[0..N] (Output phase, commTask)
 *
 * Reference: SparkFun Example20_NTRIP_Client.ino
 * Reference: NTRIP Protocol Specification, Rev 2.0
 */

#include "ntrip.h"

#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)

#include "hal/hal.h"
#include "dependency_policy.h"
#include "hw_status.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_NTRIP
#include "esp_log.h"
#include "log_ext.h"

#include <cstring>
#include <cstdio>

// ===================================================================
// Base64 encoding for NTRIP Basic Auth
// ===================================================================
// Minimal Base64 encoder (no external dependency).
// ESP32 has a built-in base64 library but it's not available on all
// platforms. This self-contained implementation avoids lib deps.

static const char k_b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/// Encode a string into Base64. Returns number of chars written (including NUL).
/// Output buffer must be at least ((input_len + 2) / 3) * 4 + 1 bytes.
static size_t base64Encode(const char* input, size_t input_len,
                           char* output, size_t output_max) {
    if (!input || !output || output_max == 0) return 0;

    size_t out_pos = 0;
    size_t i = 0;

    while (i < input_len && out_pos + 4 < output_max) {
        uint32_t octet_a = static_cast<uint8_t>(input[i++]);
        uint32_t octet_b = (i < input_len) ? static_cast<uint8_t>(input[i++]) : 0u;
        uint32_t octet_c = (i < input_len) ? static_cast<uint8_t>(input[i++]) : 0u;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[out_pos++] = k_b64_table[(triple >> 18) & 0x3F];
        output[out_pos++] = k_b64_table[(triple >> 12) & 0x3F];
        output[out_pos++] = k_b64_table[(triple >>  6) & 0x3F];
        output[out_pos++] = k_b64_table[ triple        & 0x3F];
    }

    // Padding
    size_t mod = input_len % 3;
    if (mod == 1 && out_pos + 2 < output_max) {
        output[out_pos - 1] = '=';
        output[out_pos - 2] = '=';
    } else if (mod == 2 && out_pos + 1 < output_max) {
        output[out_pos - 1] = '=';
    }

    if (out_pos < output_max) {
        output[out_pos] = '\0';
    }
    return out_pos;
}

// ===================================================================
// Internal: state machine helpers
// ===================================================================

/// HTTP response buffer for parsing caster reply.
static constexpr size_t NTRIP_HTTP_RESP_SIZE = 512;
static char s_http_resp[NTRIP_HTTP_RESP_SIZE];
static size_t s_http_resp_len = 0;

/// RTCM ring buffer for data between TCP read and UART forward.
static uint8_t s_rtcm_ring[NTRIP_RTCM_BUF_SIZE];
static size_t s_rtcm_head = 0;
static size_t s_rtcm_tail = 0;
static size_t s_rtcm_size = 0;

/// Write to RTCM ring buffer. Returns bytes written.
static size_t rtcmRingWrite(const uint8_t* data, size_t len) {
    if (len == 0) return 0;
    const size_t free_space = NTRIP_RTCM_BUF_SIZE - s_rtcm_size;
    const size_t to_copy = (len < free_space) ? len : free_space;
    if (to_copy == 0) return 0;

    const size_t first_chunk = ((s_rtcm_head + to_copy) <= NTRIP_RTCM_BUF_SIZE)
        ? to_copy
        : (NTRIP_RTCM_BUF_SIZE - s_rtcm_head);
    memcpy(&s_rtcm_ring[s_rtcm_head], data, first_chunk);

    const size_t second_chunk = to_copy - first_chunk;
    if (second_chunk > 0) {
        memcpy(&s_rtcm_ring[0], data + first_chunk, second_chunk);
    }

    s_rtcm_head = (s_rtcm_head + to_copy) % NTRIP_RTCM_BUF_SIZE;
    s_rtcm_size += to_copy;
    return to_copy;
}

/// Peek at linear readable chunk from ring buffer.
static size_t rtcmRingPeek(const uint8_t** out_ptr) {
    if (s_rtcm_size == 0) {
        *out_ptr = nullptr;
        return 0;
    }
    *out_ptr = &s_rtcm_ring[s_rtcm_tail];
    const size_t linear = NTRIP_RTCM_BUF_SIZE - s_rtcm_tail;
    return (s_rtcm_size < linear) ? s_rtcm_size : linear;
}

/// Pop bytes from ring buffer tail.
static void rtcmRingPop(size_t len) {
    if (len >= s_rtcm_size) {
        s_rtcm_head = 0;
        s_rtcm_tail = 0;
        s_rtcm_size = 0;
        return;
    }
    s_rtcm_tail = (s_rtcm_tail + len) % NTRIP_RTCM_BUF_SIZE;
    s_rtcm_size -= len;
}

/// Build the NTRIP HTTP request string.
/// Returns total request length (excluding NUL).
static size_t buildNtripRequest(char* buf, size_t buf_max) {
    const NtripConfig& cfg = g_ntrip_config;

    // Start with GET request line
    int n = snprintf(buf, buf_max,
        "GET /%s HTTP/1.0\r\n"
        "User-Agent: NTRIP ESP32Client/1.0\r\n"
        "Host: %s:%u\r\n"
        "Ntrip-Version: Ntrip/2.0\r\n",
        cfg.mountpoint, cfg.host, static_cast<unsigned>(cfg.port));

    if (n < 0 || static_cast<size_t>(n) >= buf_max) return 0;
    size_t pos = static_cast<size_t>(n);

    // Add Basic Auth if credentials are present
    if (cfg.user[0] != '\0') {
        char credentials[96];
        snprintf(credentials, sizeof(credentials), "%s:%s", cfg.user, cfg.password);

        char encoded[128];
        base64Encode(credentials, strlen(credentials), encoded, sizeof(encoded));

        n = snprintf(buf + pos, buf_max - pos,
            "Authorization: Basic %s\r\n", encoded);
        if (n < 0 || static_cast<size_t>(n) >= buf_max - pos) return 0;
        pos += static_cast<size_t>(n);
    }

    // End headers
    n = snprintf(buf + pos, buf_max - pos,
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n");
    if (n < 0 || static_cast<size_t>(n) >= buf_max - pos) return 0;
    pos += static_cast<size_t>(n);

    return pos;
}

static size_t findFirstLineEnd(const char* resp, size_t len) {
    if (!resp || len == 0) return 0;
    for (size_t i = 0; i < len; i++) {
        if (resp[i] == '\n') return i + 1;
    }
    return 0;
}

static size_t findHeaderEnd(const char* resp, size_t len) {
    if (!resp || len < 4) return 0;
    for (size_t i = 0; i + 3 < len; i++) {
        if (resp[i] == '\r' && resp[i + 1] == '\n' &&
            resp[i + 2] == '\r' && resp[i + 3] == '\n') {
            return i + 4;
        }
    }
    return 0;
}

/// Check HTTP response for success (look for "200" status code).
/// Also checks for "401" (auth failure).
/// Returns true if caster responded with 200 OK.
static bool parseNtripResponse(const char* resp, size_t len, uint8_t* out_status) {
    if (!resp || len == 0) return false;

    // Look for "ICY 200 OK" or "HTTP/1.0 200" in the response
    bool found_200 = false;
    bool found_401 = false;

    for (size_t i = 0; i + 2 < len; i++) {
        if (resp[i] == '2' && resp[i+1] == '0' && resp[i+2] == '0') {
            found_200 = true;
            break;
        }
        if (resp[i] == '4' && resp[i+1] == '0' && resp[i+2] == '1') {
            found_401 = true;
            break;
        }
    }

    if (found_401) {
        *out_status = 1;  // 1 = auth failure sentinel
        return false;
    }

    *out_status = found_200 ? 200 : 0;
    return found_200;
}

// ===================================================================
// NTRIP state machine transitions
// ===================================================================

static void ntripEnterState(NtripConnState new_state) {
    StateLock lock;
    g_ntrip.conn_state = new_state;
    g_ntrip.state_enter_ms = hal_millis();

    if (new_state == NtripConnState::IDLE ||
        new_state == NtripConnState::ERROR ||
        new_state == NtripConnState::DISCONNECTED) {
        g_ntrip.last_error[0] = '\0';
    }
}

static void ntripSetError(const char* error) {
    StateLock lock;
    if (error) {
        std::strncpy(g_ntrip.last_error, error, sizeof(g_ntrip.last_error) - 1);
        g_ntrip.last_error[sizeof(g_ntrip.last_error) - 1] = '\0';
    }
}

// ===================================================================
// Public API implementation
// ===================================================================

void ntripInit(void) {
    // Ring buffer is already zero-initialised (static storage).
    StateLock lock;
    g_ntrip = {
        NtripConnState::IDLE,
        0, 0, 0, 0, 0, 0, {}
    };
    LOGI("NTRIP", "client initialised (FEATURE_ENABLED)");
}

void ntripSetConfig(const char* host, uint16_t port,
                    const char* mountpoint,
                    const char* user, const char* password) {
    StateLock lock;
    if (host) std::strncpy(g_ntrip_config.host, host, sizeof(g_ntrip_config.host) - 1);
    if (mountpoint) std::strncpy(g_ntrip_config.mountpoint, mountpoint, sizeof(g_ntrip_config.mountpoint) - 1);
    if (user) std::strncpy(g_ntrip_config.user, user, sizeof(g_ntrip_config.user) - 1);
    if (password) std::strncpy(g_ntrip_config.password, password, sizeof(g_ntrip_config.password) - 1);
    g_ntrip_config.port = port;
    g_ntrip_config.host[sizeof(g_ntrip_config.host) - 1] = '\0';
    g_ntrip_config.mountpoint[sizeof(g_ntrip_config.mountpoint) - 1] = '\0';
    g_ntrip_config.user[sizeof(g_ntrip_config.user) - 1] = '\0';
    g_ntrip_config.password[sizeof(g_ntrip_config.password) - 1] = '\0';

    LOGI("NTRIP", "config set: %s:%u/%s (user=%s)",
         host, static_cast<unsigned>(port), mountpoint,
         (user && user[0]) ? user : "(none)");
}

void ntripSetReconnectDelay(uint32_t delay_ms) {
    StateLock lock;
    g_ntrip_config.reconnect_delay_ms = delay_ms;
}

NtripState ntripGetState(void) {
    StateLock lock;
    return g_ntrip;
}

void ntripTick(void) {
    // Snapshot current state (under lock)
    NtripConnState state;
    uint32_t state_enter_ms;
    uint32_t reconnect_delay_ms;
    {
        StateLock lock;
        state = g_ntrip.conn_state;
        state_enter_ms = g_ntrip.state_enter_ms;
        reconnect_delay_ms = g_ntrip_config.reconnect_delay_ms;
        if (reconnect_delay_ms == 0) reconnect_delay_ms = dep_policy::NTRIP_RECONNECT_DELAY_MS;
    }

    const uint32_t now = hal_millis();
    const uint32_t elapsed = now - state_enter_ms;

    switch (state) {
    case NtripConnState::IDLE:
        // Stay idle until configuration is present, then connect.
        {
            StateLock lock;
            if (g_ntrip_config.host[0] != '\0' && g_ntrip_config.mountpoint[0] != '\0') {
                ntripEnterState(NtripConnState::CONNECTING);
            }
        }
        break;

    case NtripConnState::CONNECTING: {
        // Open TCP connection to the caster.
        const char* host;
        uint16_t port;
        {
            StateLock lock;
            host = g_ntrip_config.host;
            port = g_ntrip_config.port;
        }

        LOGI("NTRIP", "connecting to %s:%u", host, static_cast<unsigned>(port));

        if (hal_tcp_connect(host, port)) {
            ntripEnterState(NtripConnState::AUTHENTICATING);
            s_http_resp_len = 0;
            LOGI("NTRIP", "TCP connected, sending auth request");
        } else {
            StateLock lock;
            g_ntrip.connect_failures++;
            ntripSetError("TCP connect failed");
            ntripEnterState(NtripConnState::ERROR);
        }
        break;
    }

    case NtripConnState::AUTHENTICATING: {
        // Send HTTP NTRIP request, then wait for response.
        if (s_http_resp_len == 0) {
            // First call in this state: send the request.
            char request[512];
            size_t req_len = buildNtripRequest(request, sizeof(request));
            if (req_len > 0) {
                size_t sent = hal_tcp_write(
                    reinterpret_cast<const uint8_t*>(request), req_len);
                if (sent < req_len) {
                    LOGW("NTRIP", "short write: %u/%u bytes",
                         static_cast<unsigned>(sent), static_cast<unsigned>(req_len));
                }
            } else {
                ntripSetError("failed to build request");
                ntripEnterState(NtripConnState::ERROR);
                break;
            }
        }

        // Read response.
        int avail = hal_tcp_available();
        if (avail < 0) {
            // Connection lost during auth
            ntripSetError("connection lost during auth");
            ntripEnterState(NtripConnState::ERROR);
            break;
        }

        if (avail > 0 && s_http_resp_len < NTRIP_HTTP_RESP_SIZE - 1) {
            uint8_t tmp[256];
            int rd = hal_tcp_read(tmp, (avail > 256) ? 256 : static_cast<size_t>(avail));
            if (rd > 0) {
                memcpy(s_http_resp + s_http_resp_len, tmp, static_cast<size_t>(rd));
                s_http_resp_len += static_cast<size_t>(rd);
                s_http_resp[s_http_resp_len] = '\0';
            }
        }

        // Accept response as soon as first status line is complete.
        // Many NTRIP v1 casters answer "ICY 200 OK\\r\\n" and immediately start streaming RTCM.
        const size_t first_line_len = findFirstLineEnd(s_http_resp, s_http_resp_len);
        if (s_http_resp_len > 0 && first_line_len > 0) {
            uint8_t http_status = 0;
            bool ok = parseNtripResponse(s_http_resp, first_line_len, &http_status);

            {
                StateLock lock;
                g_ntrip.last_http_status = http_status;
            }

            if (ok) {
                // Preserve any payload bytes that arrived in the same TCP read.
                size_t payload_offset = findHeaderEnd(s_http_resp, s_http_resp_len);
                if (payload_offset == 0) {
                    payload_offset = first_line_len;
                }
                if (payload_offset < s_http_resp_len) {
                    const size_t payload_len = s_http_resp_len - payload_offset;
                    const size_t written = rtcmRingWrite(
                        reinterpret_cast<const uint8_t*>(s_http_resp + payload_offset),
                        payload_len);
                    StateLock lock;
                    g_ntrip.rx_bytes += static_cast<uint32_t>(written);
                    if (written > 0) {
                        g_ntrip.last_rtcm_ms = hal_millis();
                    }
                }
                ntripEnterState(NtripConnState::CONNECTED);
                LOGI("NTRIP", "authenticated, receiving RTCM stream (HTTP %u)",
                     static_cast<unsigned>(http_status));
            } else {
                const char* err = (http_status == 401) ? "401 Unauthorized" : "auth failed";
                ntripSetError(err);
                ntripEnterState(NtripConnState::ERROR);
                LOGW("NTRIP", "authentication failed (HTTP %u)",
                     static_cast<unsigned>(http_status));
                hal_tcp_disconnect();
            }
        }

        // Timeout waiting for response (5s)
        if (elapsed > 5000) {
            ntripSetError("auth response timeout");
            ntripEnterState(NtripConnState::ERROR);
            hal_tcp_disconnect();
        }
        break;
    }

    case NtripConnState::CONNECTED:
        // Check if connection is still alive.
        if (!hal_tcp_connected()) {
            ntripSetError("connection lost");
            ntripEnterState(NtripConnState::DISCONNECTED);
            LOGW("NTRIP", "connection lost from caster");
            break;
        }

        // Check RTCM freshness timeout.
        {
            StateLock lock;
            uint32_t last_rtcm = g_ntrip.last_rtcm_ms;
            if (last_rtcm > 0 &&
                now - last_rtcm > dep_policy::NTRIP_RTCM_FRESHNESS_TIMEOUT_MS) {
                ntripSetError("RTCM freshness timeout");
                ntripEnterState(NtripConnState::DISCONNECTED);
                hal_tcp_disconnect();
                LOGW("NTRIP", "RTCM freshness timeout (%lu ms)",
                     static_cast<unsigned long>(now - last_rtcm));
                break;
            }
        }

        // Report GNSS subsystem status via HW monitoring.
        hwStatusSetFlag(HW_GNSS, HW_SEV_OK);
        break;

    case NtripConnState::ERROR:
    case NtripConnState::DISCONNECTED:
        // Report GNSS subsystem error via HW monitoring.
        hwStatusSetFlag(HW_GNSS, HW_SEV_WARNING);

        // Reconnect after delay.
        if (elapsed >= reconnect_delay_ms) {
            hal_tcp_disconnect();
            ntripEnterState(NtripConnState::CONNECTING);
            LOGI("NTRIP", "reconnect attempt after %lu ms",
                 static_cast<unsigned long>(elapsed));
        }
        break;
    }
}

void ntripReadRtcm(void) {
    StateLock lock;
    if (g_ntrip.conn_state != NtripConnState::CONNECTED) return;
    // Release lock before doing I/O.
    // NOTE (TASK-029): conn_state is changed by ntripTick() which now
    // runs in task_slow. A stale CONNECTED check here means one extra
    // TCP read attempt on a potentially disconnected socket — harmless
    // (hal_tcp_read returns 0 or -1, the while-loop breaks).

    // Read available data from TCP.
    uint8_t buf[512];
    while (true) {
        int rd = hal_tcp_read(buf, sizeof(buf));
        if (rd <= 0) break;

        // Write to ring buffer.
        const size_t written = rtcmRingWrite(buf, static_cast<size_t>(rd));
        if (written < static_cast<size_t>(rd)) {
            LOGW("NTRIP", "RTCM ring overflow: dropped %u bytes",
                 static_cast<unsigned>(static_cast<size_t>(rd) - written));
        }

        // Update statistics.
        StateLock lock2;
        g_ntrip.rx_bytes += static_cast<uint32_t>(rd);
        g_ntrip.last_rtcm_ms = hal_millis();
    }
}

void ntripForwardRtcm(void) {
    StateLock lock;
    if (g_ntrip.conn_state != NtripConnState::CONNECTED) return;
    if (s_rtcm_size == 0) return;
    // Release lock before doing UART I/O.

    // Forward to all LOCAL GNSS receivers.
    for (uint8_t inst = 0; inst < GNSS_RX_MAX; inst++) {
        if (!hal_gnss_uart_is_ready(inst)) continue;

        // Forward from ring buffer to this receiver.
        while (s_rtcm_size > 0) {
            const uint8_t* chunk = nullptr;
            const size_t chunk_len = rtcmRingPeek(&chunk);
            if (!chunk || chunk_len == 0) break;

            const size_t accepted = hal_gnss_uart_write(inst, chunk, chunk_len);
            if (accepted == 0) break;
            if (accepted > chunk_len) break;

            rtcmRingPop(accepted);

            StateLock lock2;
            g_ntrip.forwarded_bytes += static_cast<uint32_t>(accepted);

            if (accepted < chunk_len) {
                // Partial write — stop forwarding for this cycle.
                break;
            }
        }
    }
}

#endif // FEAT_ENABLED(FEAT_COMPILED_NTRIP)
