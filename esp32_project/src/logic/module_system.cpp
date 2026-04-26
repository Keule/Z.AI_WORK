/**
 * @file module_system.cpp
 * @brief Central module system implementation — ADR-MODULE-002.
 *
 * Manages the module registry, dependency resolution, pipeline execution,
 * and operating mode transitions.
 */

#include "module_interface.h"
#include "hal/hal.h"
#include "global_state.h"      // for g_nav.sw.paused (modeSet)
#include "runtime_config.h"  // for module_boot_disabled bitmask
#include "nvs_config.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <nvs.h>
#endif

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MOD
#include "esp_log.h"
#include "log_ext.h"

#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ===================================================================
// Forward declarations — all module ops tables
// ===================================================================
extern const ModuleOps2 mod_eth_ops;
extern const ModuleOps2 mod_wifi_ops;
extern const ModuleOps2 mod_bt_ops;
extern const ModuleOps2 mod_network_ops;
extern const ModuleOps2 mod_gnss_ops;
extern const ModuleOps2 mod_ntrip_ops;
extern const ModuleOps2 mod_imu_ops;
extern const ModuleOps2 mod_was_ops;
extern const ModuleOps2 mod_actuator_ops;
extern const ModuleOps2 mod_safety_ops;
extern const ModuleOps2 mod_steer_ops;
extern const ModuleOps2 mod_logging_ops;
extern const ModuleOps2 mod_ota_ops;
extern const ModuleOps2 mod_spi_ops;
extern const ModuleOps2 mod_spi_shared_ops;
extern const ModuleOps2 mod_remote_console_ops;

// ===================================================================
// Module registry — static array indexed by ModuleId
// ===================================================================
static ModuleRuntime s_modules[static_cast<size_t>(ModuleId::COUNT)] = {};

/// Module ops table — MUST match ModuleId order exactly.
static const ModuleOps2* const s_ops_table[] = {
    &mod_eth_ops,         // ETH
    &mod_wifi_ops,        // WIFI
    &mod_bt_ops,          // BT
    &mod_network_ops,     // NETWORK
    &mod_gnss_ops,        // GNSS
    &mod_ntrip_ops,       // NTRIP
    &mod_imu_ops,         // IMU
    &mod_was_ops,         // WAS
    &mod_actuator_ops,    // ACTUATOR
    &mod_safety_ops,      // SAFETY
    &mod_steer_ops,       // STEER
    &mod_logging_ops,     // LOGGING
    &mod_ota_ops,         // OTA
    &mod_spi_ops,         // SPI
    &mod_spi_shared_ops,  // SPI_SHARED
    &mod_remote_console_ops, // REMOTE_CONSOLE
};
static constexpr size_t kModuleCount = sizeof(s_ops_table) / sizeof(s_ops_table[0]);

// ===================================================================
// Default freshness timeouts per module (milliseconds)
// ===================================================================
static constexpr uint32_t kDefaultFreshness[] = {
    5000,   // ETH
    5000,   // WIFI
    5000,   // BT
    1000,   // NETWORK
    2000,   // GNSS
    10000,  // NTRIP
    500,    // IMU
    300,    // WAS
    1000,   // ACTUATOR
    500,    // SAFETY
    500,    // STEER
    5000,   // LOGGING
    10000,  // OTA
    0,      // SPI (infrastructure, no freshness timeout)
    0,      // SPI_SHARED (infrastructure, no freshness timeout)
    5000,   // REMOTE_CONSOLE
};

// ===================================================================
// Mode management
// ===================================================================
static OpMode s_op_mode = OpMode::CONFIG;
static portMUX_TYPE s_mode_mutex = portMUX_INITIALIZER_UNLOCKED;

// ===================================================================
// Helper: get human-readable module name
// ===================================================================
const char* moduleIdToName(ModuleId id) {
    if (id >= ModuleId::COUNT) return "???";
    return s_ops_table[static_cast<size_t>(id)]->name;
}

ModuleId moduleIdFromName(const char* name) {
    if (!name) return ModuleId::COUNT;
    for (size_t i = 0; i < kModuleCount; i++) {
        if (std::strcmp(name, s_ops_table[i]->name) == 0) {
            return static_cast<ModuleId>(i);
        }
    }
    return ModuleId::COUNT;
}

// ===================================================================
// Dependency checking
// ===================================================================
/// Check if all dependencies of a module are active.
/// Returns true if all deps are satisfied.
static bool checkDeps(const ModuleOps2* ops) {
    if (!ops->deps) return true;  // no dependencies
    for (size_t i = 0; ops->deps[i] != ModuleId::COUNT; i++) {
        ModuleId dep = ops->deps[i];
        if (dep >= ModuleId::COUNT) continue;
        if (!s_modules[static_cast<size_t>(dep)].active) {
            hal_log("MOD-SYS: %s: dependency %s not active",
                    ops->name, moduleIdToName(dep));
            return false;
        }
    }
    return true;
}

/// Check if a module is marked as boot-disabled in RuntimeConfig.
/// Boot-disabled modules are compiled in but NOT activated at boot.
/// They can be activated later via CLI: `module <name> activate`
static bool isBootDisabled(ModuleId id) {
    const auto& cfg = softConfigGet();
    const uint16_t mask = cfg.module_boot_disabled;
    return (mask & (1u << static_cast<uint8_t>(id))) != 0;
}

// ===================================================================
// NVS mode persistence (Phase 3)
// ===================================================================
#if defined(ARDUINO_ARCH_ESP32)
static void saveModeToNvs(OpMode mode) {
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_keys::NS, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, nvs_keys::OP_MODE, static_cast<uint8_t>(mode));
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static OpMode loadModeFromNvs(void) {
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t u8 = 0;
        esp_err_t err = nvs_get_u8(handle, nvs_keys::OP_MODE, &u8);
        nvs_close(handle);
        if (err == ESP_OK && u8 <= 1) {
            return static_cast<OpMode>(u8);
        }
    }
    return OpMode::CONFIG;  // default when no NVS data
}
#else
static void saveModeToNvs(OpMode) {}
static OpMode loadModeFromNvs(void) { return OpMode::CONFIG; }
#endif

// ===================================================================
// API Implementation
// ===================================================================

void moduleSysInit(void) {
    s_op_mode = loadModeFromNvs();
    hal_log("MOD-SYS: restored mode from NVS: %s", modeToString(s_op_mode));

    for (size_t i = 0; i < kModuleCount; i++) {
        s_modules[i].ops = s_ops_table[i];
        s_modules[i].active = false;
        s_modules[i].state = {};
        s_modules[i].freshness_timeout_ms = kDefaultFreshness[i];
    }

    hal_log("MOD-SYS: initialised (%u modules)", (unsigned)kModuleCount);
}

ModuleRuntime* moduleSysGet(ModuleId id) {
    if (id >= ModuleId::COUNT) return nullptr;
    return &s_modules[static_cast<size_t>(id)];
}

const ModuleOps2* moduleSysOps(ModuleId id) {
    if (id >= ModuleId::COUNT) return nullptr;
    return s_ops_table[static_cast<size_t>(id)];
}

bool moduleSysIsActive(ModuleId id) {
    const auto* m = moduleSysGet(id);
    return m ? m->active : false;
}

bool moduleSysActivate(ModuleId id) {
    auto* m = moduleSysGet(id);
    if (!m) return false;
    if (m->active) return true;  // idempotent

    const ModuleOps2* ops = m->ops;
    if (!ops->is_enabled || !ops->is_enabled()) {
        hal_log("MOD-SYS: %s: not compiled in, cannot activate", ops->name);
        return false;
    }

    // Check dependencies
    if (!checkDeps(ops)) {
        hal_log("MOD-SYS: %s: activation rejected (missing dependencies)", ops->name);
        return false;
    }

    // Activate
    if (ops->activate) ops->activate();
    m->active = true;
    m->state.detected = true;
    m->state.error_code = 0;
    hal_log("MOD-SYS: %s -> ON", ops->name);
    return true;
}

bool moduleSysDeactivate(ModuleId id) {
    auto* m = moduleSysGet(id);
    if (!m) return false;
    if (!m->active) return true;  // idempotent

    const ModuleOps2* ops = m->ops;
    if (ops->deactivate) ops->deactivate();
    m->active = false;
    m->state = {};
    hal_log("MOD-SYS: %s -> OFF", ops->name);
    return true;
}

bool moduleSysIsHealthy(ModuleId id, uint32_t now_ms) {
    const auto* m = moduleSysGet(id);
    if (!m) return false;
    if (!m->active) return false;

    const ModuleOps2* ops = m->ops;
    if (ops->is_healthy) return ops->is_healthy(now_ms);

    // Fallback: use internal ModState
    const ModState& s = m->state;
    if (!s.detected) return false;
    if (!s.quality_ok) return false;
    if (s.error_code != 0) return false;
    if ((now_ms - s.last_update_ms) > m->freshness_timeout_ms) return false;
    return true;
}

// ===================================================================
// Pipeline execution
// ===================================================================

void moduleSysRunInput(uint32_t now_ms) {
    for (size_t i = 0; i < kModuleCount; i++) {
        if (!s_modules[i].active) continue;
        const ModuleOps2* ops = s_modules[i].ops;
        if (ops->input) {
            ModuleResult r = ops->input(now_ms);
            if (r.success) {
                s_modules[i].state.last_update_ms = now_ms;
            }
        }
    }
}

void moduleSysRunProcess(uint32_t now_ms) {
    (void)now_ms;
    for (size_t i = 0; i < kModuleCount; i++) {
        if (!s_modules[i].active) continue;
        const ModuleOps2* ops = s_modules[i].ops;
        if (ops->process) {
            ops->process(now_ms);
        }
    }
}

void moduleSysRunOutput(uint32_t now_ms) {
    (void)now_ms;
    for (size_t i = 0; i < kModuleCount; i++) {
        if (!s_modules[i].active) continue;
        const ModuleOps2* ops = s_modules[i].ops;
        if (ops->output) {
            ops->output(now_ms);
        }
    }
}

// ===================================================================
// Boot activation
// ===================================================================
void moduleSysBootActivate(void) {
    hal_log("MOD-SYS: === Boot Activation ===");

    // Build activation order respecting dependencies.
    // Skip modules marked as boot-disabled in RuntimeConfig.
    static const ModuleId boot_order[] = {
        ModuleId::ETH,        // Transport layer first
        ModuleId::IMU,        // Sensors (no deps)
        ModuleId::WAS,
        ModuleId::GNSS,
        ModuleId::SAFETY,
        ModuleId::ACTUATOR,   // Depends on IMU + WAS
        ModuleId::NETWORK,   // Protocol (depends on ETH or WIFI)
        ModuleId::NTRIP,     // Services
        ModuleId::LOGGING,
        ModuleId::OTA,
        ModuleId::STEER,     // Depends on IMU + WAS + ACTUATOR + SAFETY
    };
    static constexpr size_t kBootOrderCount = sizeof(boot_order) / sizeof(boot_order[0]);

    uint8_t skipped = 0;
    for (size_t i = 0; i < kBootOrderCount; i++) {
        const ModuleId id = boot_order[i];
        if (isBootDisabled(id)) {
            hal_log("MOD-SYS: %s -> SKIP (boot-disabled)", moduleIdToName(id));
            skipped++;
            continue;
        }
        moduleSysActivate(id);
    }

    // WiFi/BT: only if ETH is not connected AND not boot-disabled (fallback)
    if (!hal_net_is_connected() && !isBootDisabled(ModuleId::WIFI)) {
        moduleSysActivate(ModuleId::WIFI);
        if (!isBootDisabled(ModuleId::BT)) {
            moduleSysActivate(ModuleId::BT);
        }
    }

    if (skipped > 0) {
        hal_log("MOD-SYS: %u module(s) boot-disabled (use 'module <name> activate' to enable)",
                (unsigned)skipped);
    }
    hal_log("MOD-SYS: === Boot Activation Complete ===");
}

// ===================================================================
// Mode management
// ===================================================================

OpMode modeGet(void) {
    portENTER_CRITICAL(&s_mode_mutex);
    OpMode m = s_op_mode;
    portEXIT_CRITICAL(&s_mode_mutex);
    return m;
}

bool modeSet(OpMode target) {
    portENTER_CRITICAL(&s_mode_mutex);
    if (target == s_op_mode) {
        portEXIT_CRITICAL(&s_mode_mutex);
        return false;
    }

    if (target == OpMode::WORK) {
        // Check steer pipeline readiness
        bool steer_ok = moduleSysIsActive(ModuleId::IMU) &&
                        moduleSysIsActive(ModuleId::WAS) &&
                        moduleSysIsActive(ModuleId::ACTUATOR) &&
                        moduleSysIsActive(ModuleId::SAFETY) &&
                        moduleSysIsActive(ModuleId::STEER);
        if (!steer_ok) {
            portEXIT_CRITICAL(&s_mode_mutex);
            hal_log("MODE: CONFIG -> WORK rejected: steer pipeline incomplete");
            return false;
        }
        s_op_mode = OpMode::WORK;
        portEXIT_CRITICAL(&s_mode_mutex);
        // Clear paused flag — used by PGN 253 switchStatus and AgIO
        {
            StateLock lock;
            g_nav.sw.paused = false;
        }
        hal_log("MODE: CONFIG -> WORK");
        saveModeToNvs(OpMode::WORK);
        return true;
    }

    if (target == OpMode::CONFIG) {
        s_op_mode = OpMode::CONFIG;
        portEXIT_CRITICAL(&s_mode_mutex);
        // Set paused flag — used by PGN 253 switchStatus and AgIO
        {
            StateLock lock;
            g_nav.sw.paused = true;
        }
        hal_log("MODE: WORK -> CONFIG");
        saveModeToNvs(OpMode::CONFIG);
        return true;
    }

    portEXIT_CRITICAL(&s_mode_mutex);
    return false;
}

const char* modeToString(OpMode mode) {
    switch (mode) {
        case OpMode::CONFIG: return "CONFIG";
        case OpMode::WORK:   return "WORK";
        default: return "?";
    }
}
