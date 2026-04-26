/**
 * @file mod_was.cpp
 * @brief Wheel Angle Sensor module implementation — ADS1118 over SPI.
 *
 * Migrated from was.cpp. Implements all 15 ModuleOps2 functions.
 * Error codes: 1=not detected, 2=read failed, 3=implausible data
 */

#include "mod_was.h"
#include "mod_spi.h"
#include "features.h"
#include "dependency_policy.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_WAS
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>

#include "cli.h"

extern Stream* s_cli_out;

// ===================================================================
// Freshness timeout
// ===================================================================
static constexpr uint32_t FRESHNESS_TIMEOUT_MS = dep_policy::STEER_ANGLE_FRESHNESS_TIMEOUT_MS;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;

// Cached sensor values (not written to g_nav — mod_steer.cpp owns SteerState output)
static float    s_cached_angle_deg  = 0.0f;
static int16_t  s_cached_raw        = 0;

// ===================================================================
// Dependencies (none — WAS is a leaf sensor)
// ===================================================================
static const ModuleId s_deps[] = { ModuleId::COUNT };   // nullptr-terminated via sentinel

// ===================================================================
// Lifecycle
// ===================================================================

static bool mod_was_is_enabled(void) {
    return feat::ads();
}

static void mod_was_activate(void) {
    s_state = {};
    // Register as SPI consumer BEFORE using SPI bus
    spi_add_consumer(ModuleId::WAS);
    hal_steer_angle_begin();
    s_state.detected = hal_steer_angle_detect();
    if (s_state.detected) {
        LOGI("WAS", "activated, ADS1118 detected on SPI");
    } else {
        LOGW("WAS", "activated but ADS1118 NOT detected (error 1)");
        s_state.error_code = 1;
    }
}

static void mod_was_deactivate(void) {
    // Unregister from SPI bus (may trigger mode switch or bus deinit)
    spi_remove_consumer(ModuleId::WAS);
    s_state = {};
    s_cached_angle_deg = 0.0f;
    s_cached_raw = 0;
    LOGI("WAS", "deactivated");
}

static bool mod_was_is_healthy(uint32_t now_ms) {
    if (!s_state.detected) {
        s_state.error_code = 1;
        return false;
    }
    if (!s_state.quality_ok) {
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

static ModuleResult mod_was_input(uint32_t now_ms) {
    if (!s_state.detected) {
        s_state.error_code = 1;
        return { false, 1 };
    }

    s_cached_angle_deg = hal_steer_angle_read_deg();
    s_cached_raw       = hal_steer_angle_read_raw();

    // Check plausibility
    const bool plausible = dep_policy::isSteerAnglePlausible(s_cached_angle_deg);
    if (!plausible) {
        s_state.error_code = 3;
        s_state.quality_ok = false;
        LOGW("WAS", "implausible angle: %.2f deg (error 3)",
             static_cast<double>(s_cached_angle_deg));
        return { false, 3 };
    }

    s_state.quality_ok     = true;
    s_state.last_update_ms = now_ms;
    s_state.error_code     = 0;
    return MOD_OK;
}

static ModuleResult mod_was_process(uint32_t /*now_ms*/) {
    // No-Op
    return MOD_OK;
}

static ModuleResult mod_was_output(uint32_t /*now_ms*/) {
    // No-Op
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================

static bool mod_was_cfg_get(const char* key, char* buf, size_t len) {
    // WAS has no runtime-configurable parameters via this interface.
    (void)key; (void)buf; (void)len;
    return false;
}

static bool mod_was_cfg_set(const char* key, const char* val) {
    (void)key; (void)val;
    return false;
}

static bool mod_was_cfg_apply(void) {
    return true;
}

static bool mod_was_cfg_save(void) {
    return true;
}

static bool mod_was_cfg_load(void) {
    return true;
}

static bool mod_was_cfg_show(void) {
    LOGI("WAS", "detected=%s quality_ok=%s error=%ld last_update=%lu "
              "angle=%.2f raw=%d",
         s_state.detected ? "yes" : "no",
         s_state.quality_ok ? "yes" : "no",
         static_cast<long>(s_state.error_code),
         static_cast<unsigned long>(s_state.last_update_ms),
         static_cast<double>(s_cached_angle_deg),
         static_cast<int>(s_cached_raw));
    return true;
}

// ===================================================================
// Diagnostic info
// ===================================================================

static void mod_was_diag_info(void) {
    uint32_t now = hal_millis();
    uint32_t ago = (s_state.last_update_ms > 0) ? (now - s_state.last_update_ms) : 0;
    if (!s_state.detected) {
        s_cli_out->printf("  Reason:    ADS1118 not detected on SPI (error 1)\n");
    } else if (s_state.error_code == 2) {
        s_cli_out->printf("  Reason:    SPI read failed (error 2)\n");
    } else if (s_state.error_code == 3) {
        s_cli_out->printf("  Reason:    implausible angle %.2f deg (error 3)\n",
            static_cast<double>(s_cached_angle_deg));
    } else if (!s_state.quality_ok || ago > FRESHNESS_TIMEOUT_MS) {
        s_cli_out->printf("  Reason:    no WAS data for %lu ms (timeout %lu ms)\n",
            (unsigned long)ago, (unsigned long)FRESHNESS_TIMEOUT_MS);
    } else {
        s_cli_out->printf("  Reason:    OK — angle=%.2f deg, raw=%d\n",
            static_cast<double>(s_cached_angle_deg), (int)s_cached_raw);
    }
}

// ===================================================================
// Debug
// ===================================================================

static bool mod_was_debug(void) {
    s_cli_out->printf("  WAS debug: detected=%s quality_ok=%s error=%ld angle=%.2f raw=%d\n",
         s_state.detected ? "yes" : "no",
         s_state.quality_ok ? "yes" : "no",
         static_cast<long>(s_state.error_code),
         static_cast<double>(s_cached_angle_deg),
         static_cast<int>(s_cached_raw));
    return s_state.detected;
}

// ===================================================================
// Public accessors (for mod_steer to consume)
// ===================================================================

/// Get cached steering angle in degrees.
float mod_was_get_angle_deg(void) {
    return s_cached_angle_deg;
}

/// Get cached raw ADC value.
int16_t mod_was_get_raw(void) {
    return s_cached_raw;
}

/// Get WAS quality flag.
bool mod_was_get_quality(void) {
    return s_state.quality_ok;
}

/// Get WAS last update timestamp.
uint32_t mod_was_get_timestamp_ms(void) {
    return s_state.last_update_ms;
}

// ===================================================================
// Ops table
// ===================================================================

const ModuleOps2 mod_was_ops = {
    /* name        */ "WAS",
    /* id          */ ModuleId::WAS,
    /* is_enabled  */ mod_was_is_enabled,
    /* activate    */ mod_was_activate,
    /* deactivate  */ mod_was_deactivate,
    /* is_healthy  */ mod_was_is_healthy,
    /* input       */ mod_was_input,
    /* process     */ mod_was_process,
    /* output      */ mod_was_output,
    /* cfg_keys   */ nullptr,
    /* cfg_get     */ mod_was_cfg_get,
    /* cfg_set     */ mod_was_cfg_set,
    /* cfg_apply   */ mod_was_cfg_apply,
    /* cfg_save    */ mod_was_cfg_save,
    /* cfg_load    */ mod_was_cfg_load,
    /* cfg_show    */ mod_was_cfg_show,
    /* diag_info   */ mod_was_diag_info,
    /* debug       */ mod_was_debug,
    /* deps        */ s_deps
};
