/**
 * @file um980_uart_setup.cpp
 * @brief Runtime setup helper for dual UM980 UART mapping.
 */

#include "um980_uart_setup.h"

#include <Arduino.h>

#include "fw_config.h"
#include "hal/hal.h"

static Um980UartSetup s_um980_setup;
static constexpr size_t CONSOLE_SEGMENT_WIDTH = 60;
static constexpr size_t CONSOLE_TOTAL_WIDTH = CONSOLE_SEGMENT_WIDTH * 2;
static char s_console_acc_a[CONSOLE_SEGMENT_WIDTH + 1] = {0};
static char s_console_acc_b[CONSOLE_SEGMENT_WIDTH + 1] = {0};
static size_t s_console_len_a = 0;
static size_t s_console_len_b = 0;
static char s_console_frame[CONSOLE_TOTAL_WIDTH + 1] = {0};
static bool s_console_dirty = false;
static Stream* s_console_mirror = nullptr;

static void resetConsoleFrame(void) {
    for (size_t i = 0; i < CONSOLE_TOTAL_WIDTH; ++i) {
        s_console_frame[i] = ' ';
    }
    s_console_frame[CONSOLE_TOTAL_WIDTH] = '\0';
}

void um980SetupLoadDefaults(uint32_t baud_default) {
    const uint32_t baud = (baud_default == 0) ? 921600u : baud_default;
    s_um980_setup.baud_a = baud;
    s_um980_setup.baud_b = baud;
    s_um980_setup.swap_a = false;
    s_um980_setup.swap_b = false;
    s_um980_setup.console_a = false;
    s_um980_setup.console_b = false;
    s_um980_setup.role_a = 0;
    s_um980_setup.role_b = 0;
    s_console_acc_a[0] = '\0';
    s_console_acc_b[0] = '\0';
    s_console_len_a = 0;
    s_console_len_b = 0;
    resetConsoleFrame();
    s_console_dirty = false;
}

Um980UartSetup um980SetupGet(void) {
    return s_um980_setup;
}

void um980SetupSetBaud(uint8_t port_idx, uint32_t baud) {
    if (baud == 0) return;
    if (port_idx == 0) {
        s_um980_setup.baud_a = baud;
    } else if (port_idx == 1) {
        s_um980_setup.baud_b = baud;
    }
}

void um980SetupSetSwap(uint8_t port_idx, bool enabled) {
    if (port_idx == 0) {
        s_um980_setup.swap_a = enabled;
    } else if (port_idx == 1) {
        s_um980_setup.swap_b = enabled;
    }
}

void um980SetupSetConsole(uint8_t port_idx, bool enabled) {
    if (port_idx == 0) {
        s_um980_setup.console_a = enabled;
    } else if (port_idx == 1) {
        s_um980_setup.console_b = enabled;
    }
}

static void updateConsoleSegment(size_t segment_start, const char* text) {
    if (!text || segment_start >= CONSOLE_TOTAL_WIDTH) return;
    const size_t segment_end = segment_start + CONSOLE_SEGMENT_WIDTH;
    for (size_t i = segment_start; i < segment_end && i < CONSOLE_TOTAL_WIDTH; ++i) {
        s_console_frame[i] = ' ';
    }

    size_t out = 0;
    while (text[out] != '\0' && out < CONSOLE_SEGMENT_WIDTH) {
        s_console_frame[segment_start + out] = text[out];
        ++out;
    }
}

static void appendConsoleByte(char* line, size_t* line_len, uint8_t byte, size_t segment_start) {
    if (!line || !line_len) return;
    if (byte == '\r') return;
    if (byte == '\n') {
        line[*line_len] = '\0';
        updateConsoleSegment(segment_start, line);
        s_console_dirty = true;
        *line_len = 0;
        return;
    }
    if (byte < 32 || byte > 126) return;
    if (*line_len + 1 >= CONSOLE_SEGMENT_WIDTH) {
        line[*line_len] = '\0';
        updateConsoleSegment(segment_start, line);
        s_console_dirty = true;
        *line_len = 0;
    }
    line[(*line_len)++] = static_cast<char>(byte);
    line[*line_len] = '\0';
}

static void pollConsole(HardwareSerial& uart, bool enabled, char* line, size_t* line_len, size_t segment_start) {
    if (!enabled) return;
    int budget = 96;
    while (budget-- > 0 && uart.available() > 0) {
        const int value = uart.read();
        if (value < 0) break;
        appendConsoleByte(line, line_len, static_cast<uint8_t>(value), segment_start);
    }
}

void um980SetupConsoleTick(void) {
    pollConsole(Serial1, s_um980_setup.console_a, s_console_acc_a, &s_console_len_a, 0);
    pollConsole(Serial2, s_um980_setup.console_b, s_console_acc_b, &s_console_len_b, CONSOLE_SEGMENT_WIDTH);
    if (!s_console_dirty) return;

    Serial.print("\r");
    Serial.print(s_console_frame);
    if (s_console_mirror) {
        s_console_mirror->print("\r");
        s_console_mirror->print(s_console_frame);
    }
    s_console_dirty = false;
}

void um980SetupSetConsoleMirror(Stream* mirror) {
    s_console_mirror = mirror;
}

void um980SetupSetRole(uint8_t port_idx, uint8_t role) {
    if (role > 3) role = 0;  // Invalid → DISABLED
    if (port_idx == 0) {
        s_um980_setup.role_a = role;
    } else if (port_idx == 1) {
        s_um980_setup.role_b = role;
    }
    // Warnung bei gleichem Rolle auf beiden UARTs
    if (port_idx == 0 && s_um980_setup.role_a != 0 && s_um980_setup.role_a == s_um980_setup.role_b) {
        Serial.println("WARNUNG: Beide UARTs haben dieselbe Rolle!");
    }
    if (port_idx == 1 && s_um980_setup.role_b != 0 && s_um980_setup.role_b == s_um980_setup.role_a) {
        Serial.println("WARNUNG: Beide UARTs haben dieselbe Rolle!");
    }
}

uint8_t um980SetupGetRole(uint8_t port_idx) {
    return (port_idx == 0) ? s_um980_setup.role_a : s_um980_setup.role_b;
}

bool um980SetupApplyPort(uint8_t port_idx) {
    if (port_idx == 0) {
        const int8_t rx = s_um980_setup.swap_a ? GNSS_UART1_TX : GNSS_UART1_RX;
        const int8_t tx = s_um980_setup.swap_a ? GNSS_UART1_RX : GNSS_UART1_TX;
        const bool ok = hal_gnss_uart_begin(0, s_um980_setup.baud_a, rx, tx);
        if (ok) s_um980_setup.console_a = true;
        hal_log("Main: UM980-A UART setup baud=%lu rx=%d tx=%d swap=%s -> %s",
                static_cast<unsigned long>(s_um980_setup.baud_a),
                static_cast<int>(rx),
                static_cast<int>(tx),
                s_um980_setup.swap_a ? "ON" : "OFF",
                ok ? "OK" : "FAIL");
        return ok;
    }
    if (port_idx == 1) {
        const int8_t rx = s_um980_setup.swap_b ? GNSS_UART2_TX : GNSS_UART2_RX;
        const int8_t tx = s_um980_setup.swap_b ? GNSS_UART2_RX : GNSS_UART2_TX;
        const bool ok = hal_gnss_uart_begin(1, s_um980_setup.baud_b, rx, tx);
        if (ok) s_um980_setup.console_b = true;
        hal_log("Main: UM980-B UART setup baud=%lu rx=%d tx=%d swap=%s -> %s",
                static_cast<unsigned long>(s_um980_setup.baud_b),
                static_cast<int>(rx),
                static_cast<int>(tx),
                s_um980_setup.swap_b ? "ON" : "OFF",
                ok ? "OK" : "FAIL");
        return ok;
    }
    return false;
}

bool um980SetupApply(void) {
    const bool a_ok = um980SetupApplyPort(0);
    const bool b_ok = um980SetupApplyPort(1);
    return a_ok && b_ok;
}
