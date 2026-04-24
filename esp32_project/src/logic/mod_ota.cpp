/**
 * @file mod_ota.cpp
 * @brief Module OTA — Over-The-Air firmware update (stub implementation).
 *
 * Minimal stub: registers OTA in the module system but defers all
 * actual OTA logic (HTTP server, SD-card update, version check) to
 * main.cpp.  Will be migrated here in a future step.
 *
 * Pipeline:
 *   input()   — No-Op  (OTA is event-driven, not polled)
 *   process() — No-Op
 *   output()  — No-Op
 *
 * Configuration:
 *   auto_check (bool) — whether to periodically check for updates
 */

#include "mod_ota.h"
#include "hal/hal.h"

#include <cstdio>
#include <cstring>

// ===================================================================
// Error codes (module-specific)
// ===================================================================

// 0 = no errors for now.  Real OTA errors will be added during migration.

// ===================================================================
// Dependencies — needs at least one transport (ETH)
// ===================================================================

static const ModuleId s_deps[] = {
    ModuleId::ETH,
    ModuleId::COUNT   // sentinel
};

// ===================================================================
// Module state
// ===================================================================

static ModState s_state;

// ===================================================================
// Configuration
// ===================================================================

/// Whether to automatically check for firmware updates.
static bool cfg_auto_check = false;

// ===================================================================
// Lifecycle: is_enabled — OTA is always available as a concept
// ===================================================================

static bool mod_ota_is_enabled(void) {
    (void)0;
    return true;
}

// ===================================================================
// Lifecycle: activate
// ===================================================================

static void mod_ota_activate(void) {
    s_state.detected = true;
    s_state.error_code = 0;
    s_state.last_update_ms = hal_millis();
    hal_log("OTA: activated (stub)");
}

// ===================================================================
// Lifecycle: deactivate — No-Op
// ===================================================================

static void mod_ota_deactivate(void) {
    s_state.detected = false;
    s_state.quality_ok = false;
    s_state.error_code = 0;
    s_state.last_update_ms = 0;
    hal_log("OTA: deactivated");
}

// ===================================================================
// Lifecycle: is_healthy — always true if active
// ===================================================================

static bool mod_ota_is_healthy(uint32_t now_ms) {
    (void)now_ms;

    if (!s_state.detected) return false;

    // OTA is event-driven — no freshness or quality concept.
    // Always healthy as long as detected.
    s_state.quality_ok = true;
    return true;
}

// ===================================================================
// Pipeline: input — No-Op
// ===================================================================

static ModuleResult mod_ota_input(uint32_t now_ms) {
    (void)now_ms;
    return MOD_OK;
}

// ===================================================================
// Pipeline: process — No-Op
// ===================================================================

static ModuleResult mod_ota_process(uint32_t now_ms) {
    (void)now_ms;
    return MOD_OK;
}

// ===================================================================
// Pipeline: output — No-Op
// ===================================================================

static ModuleResult mod_ota_output(uint32_t now_ms) {
    (void)now_ms;
    return MOD_OK;
}

// ===================================================================
// Configuration: cfg_get
// ===================================================================

static bool mod_ota_cfg_get(const char* key, char* buf, size_t len) {
    if (!key || !buf || len == 0) return false;

    if (std::strcmp(key, "auto_check") == 0) {
        std::snprintf(buf, len, "%s", cfg_auto_check ? "true" : "false");
        return true;
    }
    return false;
}

// ===================================================================
// Configuration: cfg_set
// ===================================================================

static bool mod_ota_cfg_set(const char* key, const char* val) {
    if (!key || !val) return false;

    if (std::strcmp(key, "auto_check") == 0) {
        if (std::strcmp(val, "true") == 0 || std::strcmp(val, "1") == 0) {
            cfg_auto_check = true;
        } else if (std::strcmp(val, "false") == 0 || std::strcmp(val, "0") == 0) {
            cfg_auto_check = false;
        } else {
            return false;
        }
        return true;
    }
    return false;
}

// ===================================================================
// Configuration: cfg_apply
// ===================================================================

static bool mod_ota_cfg_apply(void) {
    hal_log("OTA: config applied (auto_check=%s)",
            cfg_auto_check ? "true" : "false");
    return true;
}

// ===================================================================
// Configuration: cfg_save / cfg_load
// ===================================================================

static bool mod_ota_cfg_save(void) {
    // TODO: persist to NVS
    hal_log("OTA: cfg_save (NVS not yet implemented)");
    return true;
}

static bool mod_ota_cfg_load(void) {
    // TODO: load from NVS; for now use compile-time default
    hal_log("OTA: cfg_load (using defaults)");
    return true;
}

// ===================================================================
// Configuration: cfg_show
// ===================================================================

static bool mod_ota_cfg_show(void) {
    hal_log("OTA config:");
    hal_log("  auto_check = %s", cfg_auto_check ? "true" : "false");
    return true;
}

// ===================================================================
// Debug
// ===================================================================

static bool mod_ota_debug(void) {
    hal_log("OTA debug (stub):");
    hal_log("  detected    = %s", s_state.detected ? "yes" : "no");
    hal_log("  quality_ok  = %s", s_state.quality_ok ? "yes" : "no");
    hal_log("  error_code  = %lu", (unsigned long)s_state.error_code);
    hal_log("  last_update = %lu ms", (unsigned long)s_state.last_update_ms);
    hal_log("  auto_check  = %s", cfg_auto_check ? "true" : "false");
    return true;
}

// ===================================================================
// Ops table — const, ModuleOps2 (15 function pointers)
// ===================================================================

const ModuleOps2 mod_ota_ops = {
    .name        = "OTA",
    .id          = ModuleId::OTA,

    .is_enabled  = mod_ota_is_enabled,
    .activate    = mod_ota_activate,
    .deactivate  = mod_ota_deactivate,
    .is_healthy  = mod_ota_is_healthy,

    .input       = mod_ota_input,
    .process     = mod_ota_process,
    .output      = mod_ota_output,

    .cfg_get     = mod_ota_cfg_get,
    .cfg_set     = mod_ota_cfg_set,
    .cfg_apply   = mod_ota_cfg_apply,
    .cfg_save    = mod_ota_cfg_save,
    .cfg_load    = mod_ota_cfg_load,
    .cfg_show    = mod_ota_cfg_show,

    .debug       = mod_ota_debug,

    .deps        = s_deps,
};
