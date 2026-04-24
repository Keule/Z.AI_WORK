/**
 * @file soft_config.h
 * @brief User-editable default configuration — TASK-028.
 *
 * Compile-time defaults for all user-configurable values.
 * The RuntimeConfig (RAM copy in runtime_config.h) is loaded from
 * these defaults at boot and can be overridden later via
 * Serial / SD card / WebUI.
 *
 * To change a default, edit the value below and re-flash.
 */
#pragma once
#include <cstdint>

namespace cfg {

// --- NTRIP Client Defaults ---
// Credentials are loaded from SD card (/ntrip.cfg) at boot via
// softConfigLoadOverrides().  Empty defaults here — NTRIP stays
// in IDLE state when no credentials file is present (TASK-033).
inline constexpr const char* NTRIP_HOST        = "";
inline constexpr uint16_t     NTRIP_PORT        = 2101;
inline constexpr const char* NTRIP_MOUNTPOINT   = "";
inline constexpr const char* NTRIP_USER         = "";
inline constexpr const char* NTRIP_PASSWORD     = "";
inline constexpr uint32_t     NTRIP_RECONNECT_MS = 5000;

// --- GNSS Defaults ---
inline constexpr uint32_t     GNSS_BAUD            = 460800;
inline constexpr uint32_t     GNSS_RETRY_TIMEOUT_MS = 10000;

// --- Network Defaults ---
inline constexpr bool         NET_DHCP             = true;
inline constexpr uint8_t      NET_IP[4]            = {192, 168, 1, 70};
inline constexpr uint8_t      NET_SUBNET[4]        = {255, 255, 255, 0};
inline constexpr uint8_t      NET_GATEWAY[4]       = {192, 168, 1, 1};
inline constexpr uint8_t      NET_DNS[4]           = {8, 8, 8, 8};

// --- Logging Defaults ---
inline constexpr uint32_t     LOG_INTERVAL_MS      = 100;   // 10 Hz
inline constexpr bool         LOG_DEFAULT_ACTIVE   = false;

// --- Module Default States (ModState values: -1=unavail, 0=off, 1=on) ---
// These are only used as defaults; actual availability depends on
// firmware compilation (feat::* flags) and pin population.
inline constexpr int8_t      MOD_DEFAULT_NTRIP     = 1;     // on by default when compiled in
inline constexpr int8_t      MOD_DEFAULT_LOGSW     = 0;     // off by default (switch controlled)
inline constexpr int8_t      MOD_DEFAULT_SD        = 1;     // on by default when SD card is present at boot

}  // namespace cfg
