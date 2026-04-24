/**
 * @file um980_uart_setup.h
 * @brief Runtime setup helper for dual UM980 UART mapping.
 */

#pragma once

#include <cstdint>
#include <Stream.h>

struct Um980UartSetup {
    uint32_t baud_a = 921600;
    uint32_t baud_b = 921600;
    bool swap_a = false;
    bool swap_b = false;
    bool console_a = false;
    bool console_b = false;
    /// UART-A Rolle: 0=DISABLED, 1=NMEA, 2=RTCM, 3=DIAG
    uint8_t role_a = 0;
    /// UART-B Rolle: 0=DISABLED, 1=NMEA, 2=RTCM, 3=DIAG
    uint8_t role_b = 0;
};

/// Initialize setup state from defaults/runtime config.
void um980SetupLoadDefaults(uint32_t baud_default);

/// Get current setup state snapshot.
Um980UartSetup um980SetupGet(void);

/// Set baud rate for UM980-A (port_idx=0) or UM980-B (port_idx=1).
void um980SetupSetBaud(uint8_t port_idx, uint32_t baud);

/// Enable/disable RX/TX swap for UM980-A (port_idx=0) or UM980-B (port_idx=1).
void um980SetupSetSwap(uint8_t port_idx, bool enabled);

/// Enable/disable live UART console line for UM980-A (port_idx=0) or UM980-B (port_idx=1).
void um980SetupSetConsole(uint8_t port_idx, bool enabled);

/// Apply current setup to both UARTs. Returns true if both begin calls succeeded.
bool um980SetupApply(void);

/// Apply setup for one port only (0=A, 1=B). Returns true on success.
bool um980SetupApplyPort(uint8_t port_idx);

/// Poll enabled UART consoles and update one-line status output.
void um980SetupConsoleTick(void);

/// Optional mirror target for live UART console line (e.g. Bluetooth SPP).
void um980SetupSetConsoleMirror(Stream* mirror);

/// Set UART role for port (port_idx=0=A, port_idx=1=B).
/// role: 0=DISABLED, 1=NMEA, 2=RTCM, 3=DIAG
void um980SetupSetRole(uint8_t port_idx, uint8_t role);

/// Get UART role for port.
uint8_t um980SetupGetRole(uint8_t port_idx);
