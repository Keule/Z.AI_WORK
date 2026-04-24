/**
 * @file mod_safety.cpp
 * @brief Safety loop monitoring module implementation.
 *
 * Monitors safety circuit GPIO (hal_safety_ok) and watchdog timeout
 * from g_nav.sw.watchdog_timer_ms.
 * Error codes: 1=safety_kick, 2=watchdog_triggered
 */

#include "mod_safety.h"
#include "features.h"
#include "dependency_policy.h"
#include "global_state.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MOD
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;

// ===================================================================
// Dependencies (none — safety is a leaf sensor)
// ===================================================================
static const ModuleId s_deps[] = { ModuleId::COUNT };   // nullptr-terminated via sentinel

// ===================================================================
// Lifecycle
// ===================================================================

static bool mod_safety_is_enabled(void) {
    return feat::safety();
}

static void mod_safety_activate(void) {
    s_state = {};
    // Safety pin is always readable via hal_safety_ok()
    s_state.detected = true;
    LOGI("SAFETY", "activated, safety GPIO monitoring enabled");
}

static void mod_safety_deactivate(void) {
    // No-Op
    LOGI("SAFETY", "deactivated");
}

static bool mod_safety_is_healthy(uint32_t /*now_ms*/) {
    if (s_state.error_code != 0) {
        return false;
    }
    if (!hal_safety_ok()) {
        return false;
    }
    return true;
}

// ===================================================================
// Pipeline
// ===================================================================

static ModuleResult mod_safety_input(uint32_t now_ms) {
    const bool ok = hal_safety_ok();

    if (!ok) {
        s_state.error_code = 1;  // safety_kick
        s_state.quality_ok = false;
        LOGW("SAFETY", "SAFETY KICK detected (error 1)");
    } else {
        s_state.error_code = 0;
        s_state.quality_ok = true;
    }

    s_state.last_update_ms = now_ms;
    return { ok, ok ? 0u : 1u };
}

static ModuleResult mod_safety_process(uint32_t now_ms) {
    // Check watchdog timeout from global switch state
    uint32_t watchdog_timer_ms = 0;
    {
        StateLock lock;
        watchdog_timer_ms = g_nav.sw.watchdog_timer_ms;
    }

    if (watchdog_timer_ms > 0 &&
        (now_ms - watchdog_timer_ms) > dep_policy::WATCHDOG_TIMEOUT_MS) {
        s_state.error_code = 2;  // watchdog_triggered
        LOGW("SAFETY", "watchdog triggered: timer=%lu now=%lu (error 2)",
             static_cast<unsigned long>(watchdog_timer_ms),
             static_cast<unsigned long>(now_ms));
        return { false, 2 };
    }

    return MOD_OK;
}

static ModuleResult mod_safety_output(uint32_t /*now_ms*/) {
    // No-Op
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================

static bool mod_safety_cfg_get(const char* key, char* buf, size_t len) {
    // Safety has no configurable parameters.
    (void)key; (void)buf; (void)len;
    return false;
}

static bool mod_safety_cfg_set(const char* key, const char* val) {
    (void)key; (void)val;
    return false;
}

static bool mod_safety_cfg_apply(void) {
    return true;
}

static bool mod_safety_cfg_save(void) {
    return true;
}

static bool mod_safety_cfg_load(void) {
    return true;
}

static bool mod_safety_cfg_show(void) {
    LOGI("SAFETY", "detected=%s quality_ok=%s error=%ld last_update=%lu "
                   "safety_gpio=%s",
         s_state.detected ? "yes" : "no",
         s_state.quality_ok ? "yes" : "no",
         static_cast<long>(s_state.error_code),
         static_cast<unsigned long>(s_state.last_update_ms),
         hal_safety_ok() ? "OK" : "KICK");
    return true;
}

// ===================================================================
// Debug
// ===================================================================

static bool mod_safety_debug(void) {
    LOGI("SAFETY", "debug: detected=%s error=%ld safety_gpio=%s",
         s_state.detected ? "yes" : "no",
         static_cast<long>(s_state.error_code),
         hal_safety_ok() ? "OK" : "KICK");
    return s_state.detected;
}

// ===================================================================
// Ops table
// ===================================================================

const ModuleOps2 mod_safety_ops = {
    /* name        */ "SAFETY",
    /* id          */ ModuleId::SAFETY,
    /* is_enabled  */ mod_safety_is_enabled,
    /* activate    */ mod_safety_activate,
    /* deactivate  */ mod_safety_deactivate,
    /* is_healthy  */ mod_safety_is_healthy,
    /* input       */ mod_safety_input,
    /* process     */ mod_safety_process,
    /* output      */ mod_safety_output,
    /* cfg_get     */ mod_safety_cfg_get,
    /* cfg_set     */ mod_safety_cfg_set,
    /* cfg_apply   */ mod_safety_cfg_apply,
    /* cfg_save    */ mod_safety_cfg_save,
    /* cfg_load    */ mod_safety_cfg_load,
    /* cfg_show    */ mod_safety_cfg_show,
    /* debug       */ mod_safety_debug,
    /* deps        */ s_deps
};
