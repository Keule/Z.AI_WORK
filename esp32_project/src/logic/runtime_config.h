/**
 * @file runtime_config.h
 * @brief Mutable RAM copy of user configuration — TASK-028.
 *
 * Loaded from cfg:: defaults at boot, can be overridden at runtime
 * via Serial / SD card / WebUI.
 */
#pragma once
#include <cstdint>

/// Mutable RAM copy of user configuration.
/// Loaded from cfg:: defaults at boot, can be overridden at runtime.
struct RuntimeConfig {
    // NTRIP
    char     ntrip_host[64];
    uint16_t ntrip_port;
    char     ntrip_mountpoint[48];
    char     ntrip_user[32];
    char     ntrip_password[32];
    uint32_t ntrip_reconnect_ms;
    // PID
    float    pid_kp;
    float    pid_ki;
    float    pid_kd;
    // Network
    uint8_t  net_mode;      // 0=DHCP, 1=Static
    uint32_t net_ip;
    uint32_t net_gateway;
    uint32_t net_subnet;
    // Actuator
    uint8_t  actuator_type; // 0=SPI, 1=Cytron, 2=IBT2

    // GNSS
    uint32_t gnss_baud;
    uint32_t gnss_uart_a_baud;
    uint32_t gnss_uart_b_baud;
    uint8_t  gnss_uart_a_role;  // 0=DISABLED, 1=NMEA, 2=RTCM, 3=DIAG
    uint8_t  gnss_uart_b_role;

    // Logging
    uint32_t log_interval_ms;
    bool     log_default_active;

    // Module boot control — bitmask of modules to SKIP at boot activation.
    // Compiled-in but NOT activated. Can be activated later via CLI.
    // Uses ModuleId enum values as bit positions.
    // Example: (1u << (uint8_t)ModuleId::NTRIP) | (1u << (uint8_t)ModuleId::LOGGING)
    uint16_t module_boot_disabled;
};

/// Load cfg:: defaults into a RuntimeConfig instance.
void softConfigLoadDefaults(RuntimeConfig& cfg);

/// Load user overrides (stub — future: SD card, Serial, WebUI).
/// Returns true if overrides were loaded, false if no overrides available.
bool softConfigLoadOverrides(RuntimeConfig& cfg);

/// Get the global runtime config instance.
RuntimeConfig& softConfigGet(void);
