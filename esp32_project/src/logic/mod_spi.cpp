/**
 * @file mod_spi.cpp
 * @brief SPI Shared Bus module implementation.
 *
 * Infrastructure module that manages the sensor SPI bus lifecycle and
 * auto-switches between DIRECT (1 consumer) and SHARED (2+ consumers) modes.
 *
 * Consumers: IMU (BNO085), WAS (ADS1118), ACTUATOR (DRV8263)
 * Bus:       SPI2_HOST / SENS_SPI_BUS (SCK=47, MISO=21, MOSI=38)
 *
 * This module is auto-managed — not in boot_order, not user-activatable.
 * It activates when the first SPI consumer registers, deactivates when
 * the last consumer unregisters.
 */

#include "mod_spi.h"
#include "features.h"
#include "hal/hal.h"
#include "module_interface.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MOD
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>
#include <cstring>

#include "cli.h"

extern Stream* s_cli_out;

// ===================================================================
// Internal state
// ===================================================================

/// Bitmask of registered consumers (bits correspond to ModuleId enum values).
static uint8_t s_consumer_mask = 0;

/// Whether the SPI bus has been initialized via hal_sensor_spi_init().
static bool s_bus_initialized = false;

/// Whether we are in DIRECT mode (single consumer, no mutex).
/// true = direct mode, false = shared mode.
static bool s_direct_mode = true;

/// Module state for health tracking.
static ModState s_state;

/// No dependencies — this module manages itself.
static const ModuleId s_deps[] = { ModuleId::COUNT };

// ===================================================================
// Internal helpers
// ===================================================================

/// Count set bits in the consumer mask.
static uint8_t consumerCount(void) {
    // Manual popcount for uint8_t (avoid __builtin dependency)
    uint8_t c = s_consumer_mask;
    c = c - ((c >> 1) & 0x55);
    c = (c & 0x33) + ((c >> 2) & 0x33);
    return (c + (c >> 4)) & 0x0F;
}

/// Find the ModuleId of the sole consumer in direct mode.
/// Returns ModuleId::COUNT if not exactly one consumer.
static ModuleId soleConsumer(void) {
    if (consumerCount() != 1) return ModuleId::COUNT;
    for (uint8_t i = 0; i < static_cast<uint8_t>(ModuleId::COUNT); i++) {
        if (s_consumer_mask & (1u << i)) {
            return static_cast<ModuleId>(i);
        }
    }
    return ModuleId::COUNT;
}

/// Update the module runtime 'active' flag to reflect bus state.
static void syncRuntimeActive(void) {
    auto* rt = moduleSysGet(ModuleId::SPI_SHARED);
    if (rt) {
        rt->active = s_bus_initialized;
    }
}

// ===================================================================
// Consumer Registration API (mod_spi.h)
// ===================================================================

void spi_shared_add_consumer(ModuleId id) {
    if (id >= ModuleId::COUNT) return;
    const uint8_t bit = 1u << static_cast<uint8_t>(id);

    // Idempotent: already registered
    if (s_consumer_mask & bit) return;

    const uint8_t old_count = consumerCount();
    s_consumer_mask |= bit;
    const uint8_t new_count = consumerCount();

    if (old_count == 0 && new_count == 1) {
        // --- First consumer → initialize bus in DIRECT mode ---
        hal_sensor_spi_init();
        hal_sensor_spi_set_multi_client(false);
        s_bus_initialized = true;
        s_direct_mode = true;
        s_state = {};
        s_state.detected = true;
        s_state.quality_ok = true;
        s_state.last_update_ms = hal_millis();
        syncRuntimeActive();
        LOGI("SPI", "bus INIT, DIRECT mode (consumer: %s)", moduleIdToName(id));
    } else if (old_count == 1 && new_count >= 2) {
        // --- 2nd consumer → transition to SHARED mode ---
        hal_sensor_spi_set_multi_client(true);
        s_direct_mode = false;
        s_state.last_update_ms = hal_millis();
        LOGI("SPI", "DIRECT -> SHARED mode (%u consumers)", (unsigned)new_count);
    }
    // 3rd consumer: no mode change, just increment count
}

void spi_shared_remove_consumer(ModuleId id) {
    if (id >= ModuleId::COUNT) return;
    const uint8_t bit = 1u << static_cast<uint8_t>(id);

    // Idempotent: not registered
    if (!(s_consumer_mask & bit)) return;

    const uint8_t old_count = consumerCount();
    s_consumer_mask &= ~bit;
    const uint8_t new_count = consumerCount();

    if (old_count >= 2 && new_count == 1) {
        // --- Going from shared → direct mode ---
        hal_sensor_spi_set_multi_client(false);
        s_direct_mode = true;
        s_state.last_update_ms = hal_millis();
        ModuleId remaining = soleConsumer();
        LOGI("SPI", "SHARED -> DIRECT mode (1 consumer: %s)",
             remaining < ModuleId::COUNT ? moduleIdToName(remaining) : "none");
    } else if (old_count == 1 && new_count == 0) {
        // --- Last consumer → deinitialize bus completely ---
        hal_sensor_spi_deinit();
        s_bus_initialized = false;
        s_direct_mode = true;
        s_state = {};
        syncRuntimeActive();
        LOGI("SPI", "bus DEINIT (no consumers)");
    }
}

uint8_t spi_shared_consumer_count(void) {
    return consumerCount();
}

bool spi_shared_is_initialized(void) {
    return s_bus_initialized;
}

bool spi_shared_is_direct_mode(void) {
    return s_bus_initialized && s_direct_mode;
}

bool spi_shared_is_shared_mode(void) {
    return s_bus_initialized && !s_direct_mode;
}

const char* spi_shared_direct_consumer_name(void) {
    if (!s_direct_mode || consumerCount() != 1) return nullptr;
    ModuleId c = soleConsumer();
    if (c < ModuleId::COUNT) return moduleIdToName(c);
    return nullptr;
}

// ===================================================================
// ModuleOps2 — Lifecycle callbacks
// ===================================================================

static bool mod_spi_is_enabled(void) {
    // Available if any SPI-consuming feature is compiled in
    return feat::imu() || feat::ads() || feat::act();
}

/// Activate: called only by consumer registration, not by CLI.
/// User-facing `module SPI_SHARED activate` explains auto-management.
static void mod_spi_activate(void) {
    // If no consumers yet but bus not initialized, init anyway
    if (!s_bus_initialized) {
        hal_sensor_spi_init();
        hal_sensor_spi_set_multi_client(false);
        s_bus_initialized = true;
        s_direct_mode = true;
        s_state = {};
        s_state.detected = true;
        s_state.quality_ok = true;
        s_state.last_update_ms = hal_millis();
        syncRuntimeActive();
        LOGI("SPI", "bus INIT (manual activate, no consumers yet)");
    }
}

/// Deactivate: only allowed if no consumers registered.
static void mod_spi_deactivate(void) {
    uint8_t cnt = consumerCount();
    if (cnt > 0) {
        LOGW("SPI", "deactivate rejected: %u consumer(s) still registered", (unsigned)cnt);
        return;
    }
    if (s_bus_initialized) {
        hal_sensor_spi_deinit();
        s_bus_initialized = false;
        s_direct_mode = true;
    }
    s_state = {};
    syncRuntimeActive();
    LOGI("SPI", "bus DEINIT (manual deactivate)");
}

static bool mod_spi_is_healthy(uint32_t now_ms) {
    if (!s_bus_initialized) return false;
    if (!s_state.detected) return false;
    // SPI bus has no freshness timeout — it's infrastructure.
    // It's healthy as long as it's initialized.
    (void)now_ms;
    return true;
}

// ===================================================================
// ModuleOps2 — Pipeline (no-op for infrastructure module)
// ===================================================================

static ModuleResult mod_spi_input(uint32_t) { return MOD_OK; }
static ModuleResult mod_spi_process(uint32_t) { return MOD_OK; }
static ModuleResult mod_spi_output(uint32_t) { return MOD_OK; }

// ===================================================================
// ModuleOps2 — Configuration (no configurable keys)
// ===================================================================

static bool mod_spi_cfg_get(const char*, char*, size_t) { return false; }
static bool mod_spi_cfg_set(const char*, const char*) { return false; }
static bool mod_spi_cfg_apply(void) { return true; }
static bool mod_spi_cfg_save(void) { return true; }
static bool mod_spi_cfg_load(void) { return true; }

static bool mod_spi_cfg_show(void) {
    const uint8_t cnt = consumerCount();
    const char* mode_str = !s_bus_initialized ? "OFF" : s_direct_mode ? "DIRECT" : "SHARED";

    if (s_cli_out) {
        s_cli_out->printf("  SPI Mode:     %s\n", mode_str);
        s_cli_out->printf("  Consumers:    %u\n", (unsigned)cnt);
        s_cli_out->printf("  Bus:          %s\n", s_bus_initialized ? "INIT" : "DOWN");

        // List active consumers
        if (cnt > 0) {
            s_cli_out->print("  Devices:      ");
            for (uint8_t i = 0; i < static_cast<uint8_t>(ModuleId::COUNT); i++) {
                if (s_consumer_mask & (1u << i)) {
                    s_cli_out->printf("%s ", moduleIdToName(static_cast<ModuleId>(i)));
                }
            }
            s_cli_out->println();
        }
    }

    LOGI("SPI", "mode=%s consumers=%u bus=%s",
         mode_str, (unsigned)cnt, s_bus_initialized ? "UP" : "DOWN");
    return true;
}

// ===================================================================
// ModuleOps2 — Diagnostics
// ===================================================================

static void mod_spi_diag_info(void) {
    if (!s_cli_out) return;
    if (!s_bus_initialized) {
        s_cli_out->printf("  Reason: bus not initialized\n");
        return;
    }
    if (s_direct_mode) {
        const char* cn = spi_shared_direct_consumer_name();
        s_cli_out->printf("  Mode: DIRECT (consumer: %s)\n", cn ? cn : "?");
    } else {
        s_cli_out->printf("  Mode: SHARED (%u consumers)\n", (unsigned)consumerCount());
    }
}

static bool mod_spi_debug(void) {
    if (!s_cli_out) return s_bus_initialized;

    const uint8_t cnt = consumerCount();
    const char* mode_str = !s_bus_initialized ? "OFF" : s_direct_mode ? "DIRECT" : "SHARED";

    s_cli_out->println("=== SPI_SHARED Debug ===");

    // Bus state
    s_cli_out->printf("  Bus:        %s\n", s_bus_initialized ? "INITIALIZED" : "DOWN");
    s_cli_out->printf("  Mode:       %s\n", mode_str);
    s_cli_out->printf("  Consumers:  %u\n", (unsigned)cnt);

    // Consumer list with status
    s_cli_out->println("  Registered:");
    bool any = false;
    for (uint8_t i = 0; i < static_cast<uint8_t>(ModuleId::COUNT); i++) {
        if (s_consumer_mask & (1u << i)) {
            ModuleId mid = static_cast<ModuleId>(i);
            const bool active = moduleSysIsActive(mid);
            s_cli_out->printf("    %-10s %s\n", moduleIdToName(mid), active ? "ACTIVE" : "INACTIVE(!)");
            any = true;
        }
    }
    if (!any) {
        s_cli_out->println("    (none)");
    }

    // Direct mode info
    if (s_direct_mode && cnt == 1) {
        const char* cn = spi_shared_direct_consumer_name();
        s_cli_out->printf("  Direct:     single consumer (%s), no mutex, no CS arbitration\n",
                         cn ? cn : "?");
    }

    // Shared mode info
    if (!s_direct_mode && cnt >= 2) {
        s_cli_out->println("  Shared:     multi-client mode, mutex + CS arbitration active");
    }

    // Telemetry
    HalSpiTelemetry tm;
    hal_sensor_spi_get_telemetry(&tm);
    s_cli_out->printf("  Telemetry:\n");
    s_cli_out->printf("    Window:     %lu ms\n", (unsigned long)tm.window_ms);
    s_cli_out->printf("    Busy:       %lu us (%.1f%%)\n",
                      (unsigned long)tm.bus_busy_us, (double)tm.bus_utilization_pct);
    s_cli_out->printf("    Transfers:  %lu\n", (unsigned long)tm.bus_transactions);
    s_cli_out->printf("    Switches:   %lu (WAS->IMU=%lu IMU->WAS=%lu)\n",
                      (unsigned long)tm.client_switches,
                      (unsigned long)tm.was_to_imu_switches,
                      (unsigned long)tm.imu_to_was_switches);
    s_cli_out->printf("    Deadline miss: IMU=%lu WAS=%lu\n",
                      (unsigned long)tm.imu_deadline_miss,
                      (unsigned long)tm.was_deadline_miss);

    return s_bus_initialized;
}

// ===================================================================
// ModuleOps2 table
// ===================================================================

const ModuleOps2 mod_spi_shared_ops = {
    "SPI_SHARED", ModuleId::SPI_SHARED,
    mod_spi_is_enabled, mod_spi_activate, mod_spi_deactivate, mod_spi_is_healthy,
    mod_spi_input, mod_spi_process, mod_spi_output,
    nullptr,  // cfg_keys (no configurable keys)
    mod_spi_cfg_get, mod_spi_cfg_set, mod_spi_cfg_apply,
    mod_spi_cfg_save, mod_spi_cfg_load, mod_spi_cfg_show,
    mod_spi_diag_info, mod_spi_debug,
    s_deps  // no dependencies
};
