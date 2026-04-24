/**
 * @file mod_gnss.cpp
 * @brief GNSS module implementation — UM980 dual UART management.
 *
 * New module for initialising GNSS receiver UARTs and reading NMEA data.
 * Error codes: 1=uart_a_init_failed, 2=uart_b_init_failed
 */

#include "mod_gnss.h"
#include "features.h"
#include "dependency_policy.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MOD
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>
#include <cstring>

// ===================================================================
// Freshness timeout (GNSS NMEA data should arrive at 10 Hz)
// ===================================================================
static constexpr uint32_t FRESHNESS_TIMEOUT_MS = 1000;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;

/// UART A ready flag
static bool s_uart_a_ready = false;
/// UART B ready flag
static bool s_uart_b_ready = false;

// ===================================================================
// Configuration keys
// ===================================================================
static uint32_t s_cfg_baud_a = 115200;
static uint32_t s_cfg_baud_b = 115200;
// role_a / role_b: 0 = primary, 1 = RTCM-only (future use)
static uint8_t  s_cfg_role_a = 0;
static uint8_t  s_cfg_role_b = 1;

// ===================================================================
// Dependencies (none — GNSS is a leaf sensor)
// ===================================================================
static const ModuleId s_deps[] = { ModuleId::COUNT };   // nullptr-terminated via sentinel

// ===================================================================
// Lifecycle
// ===================================================================

static bool mod_gnss_is_enabled(void) {
    return feat::gnss();
}

static void mod_gnss_activate(void) {
    s_state = {};

    // UART A (instance 0) — primary GNSS receiver
    // Pins are board-specific; pass -1 for rx_pin to use default.
    s_uart_a_ready = hal_gnss_uart_begin(0, s_cfg_baud_a, -1, -1);
    if (!s_uart_a_ready) {
        LOGW("GNSS", "UART-A init failed (error 1)");
        s_state.error_code = 1;
    }

    // UART B (instance 1) — secondary / RTCM receiver
    s_uart_b_ready = hal_gnss_uart_begin(1, s_cfg_baud_b, -1, -1);
    if (!s_uart_b_ready) {
        LOGW("GNSS", "UART-B init failed (error 2)");
        // Only overwrite error code if not already set for UART-A
        if (s_state.error_code == 0) {
            s_state.error_code = 2;
        }
    }

    s_state.detected = s_uart_a_ready || s_uart_b_ready;
    LOGI("GNSS", "activated: uart_a=%s uart_b=%s",
         s_uart_a_ready ? "OK" : "FAIL",
         s_uart_b_ready ? "OK" : "FAIL");
}

static void mod_gnss_deactivate(void) {
    // Close UARTs by marking them as not ready.
    // (HAL UART deinit would go here if available.)
    s_uart_a_ready = false;
    s_uart_b_ready = false;
    LOGI("GNSS", "deactivated");
}

static bool mod_gnss_is_healthy(uint32_t now_ms) {
    if (!s_uart_a_ready && !s_uart_b_ready) {
        s_state.error_code = 1;
        return false;
    }
    if (s_state.error_code != 0) {
        return false;
    }
    if (!dep_policy::isFresh(now_ms, s_state.last_update_ms, FRESHNESS_TIMEOUT_MS)) {
        return false;
    }
    return true;
}

// ===================================================================
// Pipeline
// ===================================================================

static ModuleResult mod_gnss_input(uint32_t now_ms) {
    // Read UART data from GNSS receivers.
    // NMEA parsing would go here (future implementation).
    // For now, just check that UARTs are still ready.

    bool got_data = false;

    if (s_uart_a_ready && hal_gnss_uart_is_ready(0)) {
        // Read NMEA from UART A (future: parse into g_nav.gnss)
        // uint8_t buf[256];
        // int rd = hal_uart_read(0, buf, sizeof(buf));
        got_data = true;
    }

    if (s_uart_b_ready && hal_gnss_uart_is_ready(1)) {
        // Read NMEA from UART B (future: parse)
        got_data = true;
    }

    if (got_data) {
        s_state.last_update_ms = now_ms;
        s_state.quality_ok = true;
    }

    return MOD_OK;
}

static ModuleResult mod_gnss_process(uint32_t /*now_ms*/) {
    // No-Op for now (future: NMEA sentence assembly, fix quality extraction)
    return MOD_OK;
}

static ModuleResult mod_gnss_output(uint32_t /*now_ms*/) {
    // No-Op for now (future: could output RTCM correction requests)
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================

static bool mod_gnss_cfg_get(const char* key, char* buf, size_t len) {
    if (std::strcmp(key, "baud_a") == 0) {
        std::snprintf(buf, len, "%lu", static_cast<unsigned long>(s_cfg_baud_a));
        return true;
    }
    if (std::strcmp(key, "baud_b") == 0) {
        std::snprintf(buf, len, "%lu", static_cast<unsigned long>(s_cfg_baud_b));
        return true;
    }
    if (std::strcmp(key, "role_a") == 0) {
        std::snprintf(buf, len, "%u", static_cast<unsigned>(s_cfg_role_a));
        return true;
    }
    if (std::strcmp(key, "role_b") == 0) {
        std::snprintf(buf, len, "%u", static_cast<unsigned>(s_cfg_role_b));
        return true;
    }
    return false;
}

static bool mod_gnss_cfg_set(const char* key, const char* val) {
    if (std::strcmp(key, "baud_a") == 0) {
        uint32_t v = static_cast<uint32_t>(std::strtoul(val, nullptr, 10));
        if (v > 0) { s_cfg_baud_a = v; return true; }
        return false;
    }
    if (std::strcmp(key, "baud_b") == 0) {
        uint32_t v = static_cast<uint32_t>(std::strtoul(val, nullptr, 10));
        if (v > 0) { s_cfg_baud_b = v; return true; }
        return false;
    }
    if (std::strcmp(key, "role_a") == 0) {
        s_cfg_role_a = static_cast<uint8_t>(std::strtoul(val, nullptr, 10));
        return true;
    }
    if (std::strcmp(key, "role_b") == 0) {
        s_cfg_role_b = static_cast<uint8_t>(std::strtoul(val, nullptr, 10));
        return true;
    }
    return false;
}

static bool mod_gnss_cfg_apply(void) {
    // Re-init UARTs with new baud rates (requires deactivate + activate).
    return true;
}

static bool mod_gnss_cfg_save(void) {
    // Future: persist to NVS
    return true;
}

static bool mod_gnss_cfg_load(void) {
    // Future: load from NVS
    return true;
}

static bool mod_gnss_cfg_show(void) {
    LOGI("GNSS", "detected=%s error=%ld last_update=%lu "
              "uart_a=%s uart_b=%s "
              "baud_a=%lu baud_b=%lu role_a=%u role_b=%u",
         s_state.detected ? "yes" : "no",
         static_cast<long>(s_state.error_code),
         static_cast<unsigned long>(s_state.last_update_ms),
         s_uart_a_ready ? "OK" : "DOWN",
         s_uart_b_ready ? "OK" : "DOWN",
         static_cast<unsigned long>(s_cfg_baud_a),
         static_cast<unsigned long>(s_cfg_baud_b),
         static_cast<unsigned>(s_cfg_role_a),
         static_cast<unsigned>(s_cfg_role_b));
    return true;
}

// ===================================================================
// Debug
// ===================================================================

static bool mod_gnss_debug(void) {
    LOGI("GNSS", "debug: uart_a=%s uart_b=%s baud_a=%lu baud_b=%lu",
         s_uart_a_ready ? "OK" : "DOWN",
         s_uart_b_ready ? "OK" : "DOWN",
         static_cast<unsigned long>(s_cfg_baud_a),
         static_cast<unsigned long>(s_cfg_baud_b));
    return s_state.detected;
}

// ===================================================================
// Config key definitions
// ===================================================================
static const CfgKeyDef s_gnss_keys[] = {
    {"baud_a", "UART-A baud rate"},
    {"baud_b", "UART-B baud rate"},
    {"role_a", "UART-A role (0=primary, 1=RTCM)"},
    {"role_b", "UART-B role (0=primary, 1=RTCM)"},
    {nullptr, nullptr}  // sentinel
};

static const CfgKeyDef* mod_gnss_cfg_keys(void) { return s_gnss_keys; }

// ===================================================================
// Ops table
// ===================================================================

const ModuleOps2 mod_gnss_ops = {
    /* name        */ "GNSS",
    /* id          */ ModuleId::GNSS,
    /* is_enabled  */ mod_gnss_is_enabled,
    /* activate    */ mod_gnss_activate,
    /* deactivate  */ mod_gnss_deactivate,
    /* is_healthy  */ mod_gnss_is_healthy,
    /* input       */ mod_gnss_input,
    /* process     */ mod_gnss_process,
    /* output      */ mod_gnss_output,
    /* cfg_keys   */ mod_gnss_cfg_keys,
    /* cfg_get     */ mod_gnss_cfg_get,
    /* cfg_set     */ mod_gnss_cfg_set,
    /* cfg_apply   */ mod_gnss_cfg_apply,
    /* cfg_save    */ mod_gnss_cfg_save,
    /* cfg_load    */ mod_gnss_cfg_load,
    /* cfg_show    */ mod_gnss_cfg_show,
    /* debug       */ mod_gnss_debug,
    /* deps        */ s_deps
};
