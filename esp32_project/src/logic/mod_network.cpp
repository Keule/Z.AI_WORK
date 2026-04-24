/**
 * @file mod_network.cpp
 * @brief Protocol module: PGN Codec RX/TX over UDP (ModuleId::NETWORK).
 *
 * This is the protocol layer that handles PGN encoding/decoding for
 * communication with AgOpenGPS. It delegates physical transport to
 * ETH and/or WIFI modules.
 *
 * Currently wraps the existing net.h functions (netPollReceive,
 * netSendAogFrames). Full migration into this module will happen later.
 */

#include "mod_network.h"
#include "hal/hal.h"
#include "nvs_config.h"
#include "net.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_NET
#include "esp_log.h"
#include "log_ext.h"

#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <nvs.h>
#endif

static const char* const TAG = "MOD_NETWORK";

// ===================================================================
// Error codes
// ===================================================================
static constexpr int32_t ERR_NO_TRANSPORT = 1;

// ===================================================================
// Freshness timeout for health check
// ===================================================================
static constexpr uint32_t FRESHNESS_MS = 5000;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;
static ModuleRuntime* s_rt = nullptr;

// ===================================================================
// Config keys (NVS, namespace "agsteer")
// ===================================================================
// Network protocol has no user-configurable keys currently.
// All PGN config comes from AgIO (runtime), not from NVS.
// Kept for interface completeness.

// ===================================================================
// Dependencies: requires at least ETH
// ===================================================================
static const ModuleId s_deps[] = {
    ModuleId::ETH,
    ModuleId::COUNT   // nullptr-terminated sentinel
};

// ===================================================================
// Helper: check if at least one transport is healthy
// ===================================================================
static bool anyTransportHealthy(void) {
    // Check ETH
    if (moduleSysIsActive(ModuleId::ETH) &&
        moduleSysIsHealthy(ModuleId::ETH, hal_millis())) {
        return true;
    }
    // Check WIFI (fallback)
    if (moduleSysIsActive(ModuleId::WIFI) &&
        moduleSysIsHealthy(ModuleId::WIFI, hal_millis())) {
        return true;
    }
    return false;
}

// ===================================================================
// Lifecycle
// ===================================================================
static bool network_is_enabled(void) {
    return true;
}

static void network_activate(void) {
    s_rt = moduleSysGet(ModuleId::NETWORK);
    if (!s_rt) {
        LOGE(TAG, "moduleSysGet(NETWORK) returned nullptr");
        return;
    }

    // Detected if at least one transport module is active
    s_state.detected = moduleSysIsActive(ModuleId::ETH) ||
                       moduleSysIsActive(ModuleId::WIFI);

    if (s_state.detected) {
        LOGI(TAG, "activated — transport available (ETH=%d WIFI=%d)",
             moduleSysIsActive(ModuleId::ETH),
             moduleSysIsActive(ModuleId::WIFI));
    } else {
        LOGW(TAG, "activated — no transport available");
        s_state.error_code = ERR_NO_TRANSPORT;
    }

    s_state.last_update_ms = hal_millis();
    if (s_rt) s_rt->state = s_state;
}

static void network_deactivate(void) {
    LOGI(TAG, "deactivated");
    s_state.detected = false;
    s_state.quality_ok = false;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
}

static bool network_is_healthy(uint32_t now_ms) {
    if (!s_state.detected) {
        // Re-check: maybe a transport came online
        s_state.detected = moduleSysIsActive(ModuleId::ETH) ||
                           moduleSysIsActive(ModuleId::WIFI);
        if (!s_state.detected) {
            s_state.error_code = ERR_NO_TRANSPORT;
            s_state.quality_ok = false;
            if (s_rt) s_rt->state = s_state;
            return false;
        }
    }

    // Need at least one healthy transport
    if (!anyTransportHealthy()) {
        s_state.quality_ok = false;
        s_state.error_code = ERR_NO_TRANSPORT;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

    // Freshness check
    if ((now_ms - s_state.last_update_ms) > FRESHNESS_MS) {
        s_state.quality_ok = false;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

    s_state.quality_ok = true;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
    return true;
}

// ===================================================================
// Pipeline
// ===================================================================
static ModuleResult network_input(uint32_t now_ms) {
    // Guard: only process if a transport is available
    if (!anyTransportHealthy()) {
        return {true, 0};  // Not an error, just nothing to do
    }

    // Delegate to existing netPollReceive (PGN RX)
    netPollReceive();

    s_state.last_update_ms = now_ms;
    s_state.quality_ok = true;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
    return MOD_OK;
}

static ModuleResult network_process(uint32_t /*now_ms*/) {
    // No-Op: PGN processing happens inside netPollReceive
    return MOD_OK;
}

static ModuleResult network_output(uint32_t now_ms) {
    // Guard: only send if a transport is available
    if (!anyTransportHealthy()) {
        return {true, 0};
    }

    // Delegate to existing netSendAogFrames (PGN TX)
    netSendAogFrames();

    s_state.last_update_ms = now_ms;
    s_state.quality_ok = true;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================
static bool network_cfg_get(const char* key, char* buf, size_t len) {
    // Network protocol module has no user-configurable NVS keys.
    // PGN configuration comes from AgIO at runtime.
    // For interface completeness, return false for all keys.
    (void)key; (void)buf; (void)len;
    return false;
}

static bool network_cfg_set(const char* key, const char* val) {
    // No configurable keys
    (void)key; (void)val;
    return false;
}

static bool network_cfg_apply(void) {
    // No config to apply
    return true;
}

static bool network_cfg_save(void) {
    // No config to save
    return true;
}

static bool network_cfg_load(void) {
    // No config to load
    return true;
}

static bool network_cfg_show(void) {
    LOGI(TAG, "config: (no user-configurable keys — PGN config from AgIO)");
    LOGI(TAG, "  detected=%d quality_ok=%d error=%ld",
         s_state.detected, s_state.quality_ok, (long)s_state.error_code);
    LOGI(TAG, "  ETH active=%d healthy=%d",
         moduleSysIsActive(ModuleId::ETH),
         moduleSysIsHealthy(ModuleId::ETH, hal_millis()));
    LOGI(TAG, "  WIFI active=%d healthy=%d",
         moduleSysIsActive(ModuleId::WIFI),
         moduleSysIsHealthy(ModuleId::WIFI, hal_millis()));
    return true;
}

// ===================================================================
// Debug
// ===================================================================
static bool network_debug(void) {
    LOGI(TAG, "=== NETWORK Debug ===");
    network_cfg_show();
    return true;
}

// ===================================================================
// Ops table
// ===================================================================
const ModuleOps2 mod_network_ops = {
    .name      = "NETWORK",
    .id        = ModuleId::NETWORK,

    .is_enabled  = network_is_enabled,
    .activate    = network_activate,
    .deactivate  = network_deactivate,
    .is_healthy  = network_is_healthy,

    .input   = network_input,
    .process = network_process,
    .output  = network_output,

    .cfg_get   = network_cfg_get,
    .cfg_set   = network_cfg_set,
    .cfg_apply = network_cfg_apply,
    .cfg_save  = network_cfg_save,
    .cfg_load  = network_cfg_load,
    .cfg_show  = network_cfg_show,

    .debug = network_debug,

    .deps = s_deps,
};
