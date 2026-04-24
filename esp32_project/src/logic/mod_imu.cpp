/**
 * @file mod_imu.cpp
 * @brief IMU module implementation — BNO085 over SPI.
 *
 * Migrated from imu.cpp. Implements all 15 ModuleOps2 functions.
 * Error codes: 1=not detected, 2=read failed, 3=implausible data
 */

#include "mod_imu.h"
#include "features.h"
#include "dependency_policy.h"
#include "global_state.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_IMU
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>

// ===================================================================
// Freshness timeout
// ===================================================================
static constexpr uint32_t FRESHNESS_TIMEOUT_MS = dep_policy::IMU_FRESHNESS_TIMEOUT_MS;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;

// ===================================================================
// Dependencies (none — IMU is a leaf sensor)
// ===================================================================
static const ModuleId s_deps[] = { ModuleId::COUNT };   // nullptr-terminated via sentinel

// ===================================================================
// Lifecycle
// ===================================================================

static bool mod_imu_is_enabled(void) {
    return feat::imu();
}

static void mod_imu_activate(void) {
    s_state = {};
    hal_imu_begin();
    s_state.detected = hal_imu_detect();
    if (s_state.detected) {
        LOGI("IMU", "activated, BNO085 detected on SPI");
    } else {
        LOGW("IMU", "activated but BNO085 NOT detected (error 1)");
        s_state.error_code = 1;
    }
}

static void mod_imu_deactivate(void) {
    // No-Op: SPI bus is shared and managed by HAL.
    LOGI("IMU", "deactivated (SPI bus managed by HAL)");
}

static bool mod_imu_is_healthy(uint32_t now_ms) {
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

static ModuleResult mod_imu_input(uint32_t now_ms) {
    if (!s_state.detected) {
        s_state.error_code = 1;
        return { false, 1 };
    }

    float yaw_rate = 0.0f;
    float roll = 0.0f;
    float heading = 9999.0f;

    if (!hal_imu_read(&yaw_rate, &roll, &heading)) {
        s_state.error_code = 2;
        s_state.quality_ok = false;
        LOGW("IMU", "SPI read failed (error 2)");
        return { false, 2 };
    }

    // Plausibility checks
    const bool plausible = dep_policy::isImuPlausible(yaw_rate, roll);
    const bool heading_plausible = dep_policy::isHeadingPlausible(heading);

    if (!plausible) {
        s_state.error_code = 3;
        s_state.quality_ok = false;
        LOGW("IMU", "implausible data: yaw_rate=%.2f roll=%.2f (error 3)",
             static_cast<double>(yaw_rate), static_cast<double>(roll));
        return { false, 3 };
    }

    // Update global navigation state under lock
    {
        StateLock lock;
        if (heading_plausible) {
            g_nav.imu.heading_deg = heading;
            g_nav.imu.heading_timestamp_ms = now_ms;
            g_nav.imu.heading_quality_ok = true;
        }
        g_nav.imu.yaw_rate_dps = yaw_rate;
        g_nav.imu.roll_deg = roll;
        g_nav.imu.imu_timestamp_ms = now_ms;
        g_nav.imu.imu_quality_ok = true;
    }

    s_state.quality_ok = plausible;
    s_state.last_update_ms = now_ms;
    s_state.error_code = 0;
    return MOD_OK;
}

static ModuleResult mod_imu_process(uint32_t /*now_ms*/) {
    // No-Op
    return MOD_OK;
}

static ModuleResult mod_imu_output(uint32_t /*now_ms*/) {
    // No-Op
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================

static bool mod_imu_cfg_get(const char* key, char* buf, size_t len) {
    // IMU has no runtime-configurable parameters.
    (void)key; (void)buf; (void)len;
    return false;
}

static bool mod_imu_cfg_set(const char* key, const char* val) {
    (void)key; (void)val;
    return false;
}

static bool mod_imu_cfg_apply(void) {
    return true;
}

static bool mod_imu_cfg_save(void) {
    return true;
}

static bool mod_imu_cfg_load(void) {
    return true;
}

static bool mod_imu_cfg_show(void) {
    LOGI("IMU", "detected=%s quality_ok=%s error=%ld last_update=%lu",
         s_state.detected ? "yes" : "no",
         s_state.quality_ok ? "yes" : "no",
         static_cast<long>(s_state.error_code),
         static_cast<unsigned long>(s_state.last_update_ms));
    return true;
}

// ===================================================================
// Debug
// ===================================================================

static bool mod_imu_debug(void) {
    LOGI("IMU", "debug: detected=%s quality_ok=%s error=%ld last_update=%lu",
         s_state.detected ? "yes" : "no",
         s_state.quality_ok ? "yes" : "no",
         static_cast<long>(s_state.error_code),
         static_cast<unsigned long>(s_state.last_update_ms));
    return s_state.detected;
}

// ===================================================================
// Ops table
// ===================================================================

const ModuleOps2 mod_imu_ops = {
    /* name        */ "IMU",
    /* id          */ ModuleId::IMU,
    /* is_enabled  */ mod_imu_is_enabled,
    /* activate    */ mod_imu_activate,
    /* deactivate  */ mod_imu_deactivate,
    /* is_healthy  */ mod_imu_is_healthy,
    /* input       */ mod_imu_input,
    /* process     */ mod_imu_process,
    /* output      */ mod_imu_output,
    /* cfg_keys   */ nullptr,
    /* cfg_get     */ mod_imu_cfg_get,
    /* cfg_set     */ mod_imu_cfg_set,
    /* cfg_apply   */ mod_imu_cfg_apply,
    /* cfg_save    */ mod_imu_cfg_save,
    /* cfg_load    */ mod_imu_cfg_load,
    /* cfg_show    */ mod_imu_cfg_show,
    /* debug       */ mod_imu_debug,
    /* deps        */ s_deps
};
