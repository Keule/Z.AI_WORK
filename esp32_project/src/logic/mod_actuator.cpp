/**
 * @file mod_actuator.cpp
 * @brief Steering actuator module implementation — DRV8263 SPI.
 *
 * Migrated from actuator.cpp. Implements all 15 ModuleOps2 functions.
 * Error codes: 1=not detected, 2=write failed
 */

#include "mod_actuator.h"
#include "features.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_ACT
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;

/// Current command value — set by mod_steer, written in output().
static uint16_t s_current_cmd = 0;

// ===================================================================
// Dependencies: requires IMU, WAS for steering control
// ===================================================================
static const ModuleId s_deps[] = {
    ModuleId::IMU,
    ModuleId::WAS,
    ModuleId::COUNT   // nullptr-terminated via sentinel
};

// ===================================================================
// Lifecycle
// ===================================================================

static bool mod_actuator_is_enabled(void) {
    return feat::act();
}

static void mod_actuator_activate(void) {
    s_state = {};
    hal_actuator_begin();
    s_state.detected = hal_actuator_detect();
    s_current_cmd = 0;
    if (s_state.detected) {
        LOGI("ACT", "activated, DRV8263 detected on SPI");
    } else {
        LOGW("ACT", "activated but DRV8263 NOT detected (error 1)");
        s_state.error_code = 1;
    }
}

static void mod_actuator_deactivate(void) {
    // No-Op: SPI bus is shared and managed by HAL.
    s_current_cmd = 0;
    LOGI("ACT", "deactivated (SPI bus managed by HAL)");
}

static bool mod_actuator_is_healthy(uint32_t /*now_ms*/) {
    // Actuator has no freshness concept — healthy if detected and no error.
    if (!s_state.detected) {
        s_state.error_code = 1;
        return false;
    }
    if (s_state.error_code != 0) {
        return false;
    }
    return true;
}

// ===================================================================
// Pipeline
// ===================================================================

static ModuleResult mod_actuator_input(uint32_t /*now_ms*/) {
    // No-Op: actuator receives commands from mod_steer, not from sensors.
    return MOD_OK;
}

static ModuleResult mod_actuator_process(uint32_t /*now_ms*/) {
    // No-Op: command transformation is done by mod_steer (PID).
    return MOD_OK;
}

static ModuleResult mod_actuator_output(uint32_t /*now_ms*/) {
    if (!s_state.detected) {
        s_state.error_code = 1;
        return { false, 1 };
    }

    hal_actuator_write(s_current_cmd);
    s_state.error_code = 0;
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================

static bool mod_actuator_cfg_get(const char* key, char* buf, size_t len) {
    (void)key; (void)buf; (void)len;
    return false;
}

static bool mod_actuator_cfg_set(const char* key, const char* val) {
    (void)key; (void)val;
    return false;
}

static bool mod_actuator_cfg_apply(void) {
    return true;
}

static bool mod_actuator_cfg_save(void) {
    return true;
}

static bool mod_actuator_cfg_load(void) {
    return true;
}

static bool mod_actuator_cfg_show(void) {
    LOGI("ACT", "detected=%s error=%ld cmd=%u",
         s_state.detected ? "yes" : "no",
         static_cast<long>(s_state.error_code),
         static_cast<unsigned>(s_current_cmd));
    return true;
}

// ===================================================================
// Debug
// ===================================================================

static bool mod_actuator_debug(void) {
    LOGI("ACT", "debug: detected=%s error=%ld cmd=%u",
         s_state.detected ? "yes" : "no",
         static_cast<long>(s_state.error_code),
         static_cast<unsigned>(s_current_cmd));
    return s_state.detected;
}

// ===================================================================
// Public accessors
// ===================================================================

void mod_actuator_set_cmd(uint16_t cmd) {
    s_current_cmd = cmd;
}

uint16_t mod_actuator_get_cmd(void) {
    return s_current_cmd;
}

// ===================================================================
// Ops table
// ===================================================================

const ModuleOps2 mod_actuator_ops = {
    /* name        */ "ACTUATOR",
    /* id          */ ModuleId::ACTUATOR,
    /* is_enabled  */ mod_actuator_is_enabled,
    /* activate    */ mod_actuator_activate,
    /* deactivate  */ mod_actuator_deactivate,
    /* is_healthy  */ mod_actuator_is_healthy,
    /* input       */ mod_actuator_input,
    /* process     */ mod_actuator_process,
    /* output      */ mod_actuator_output,
    /* cfg_keys   */ nullptr,
    /* cfg_get     */ mod_actuator_cfg_get,
    /* cfg_set     */ mod_actuator_cfg_set,
    /* cfg_apply   */ mod_actuator_cfg_apply,
    /* cfg_save    */ mod_actuator_cfg_save,
    /* cfg_load    */ mod_actuator_cfg_load,
    /* cfg_show    */ mod_actuator_cfg_show,
    /* debug       */ mod_actuator_debug,
    /* deps        */ s_deps
};
