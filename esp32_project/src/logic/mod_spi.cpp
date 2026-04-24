/**
 * @file mod_spi.cpp
 * @brief SPI and SPI_SHARED module implementation.
 *
 * Two infrastructure modules managing the sensor SPI bus lifecycle:
 *
 *   SPI:
 *     activate  → hal_sensor_spi_init() + DIRECT mode
 *     deactivate → hal_sensor_spi_deinit()
 *     No dependencies, pipeline is no-op.
 *     Auto-managed by consumer registration (not in boot_order).
 *
 *   SPI_SHARED:
 *     activate  → hal_sensor_spi_set_multi_client(true)
 *     deactivate → hal_sensor_spi_set_multi_client(false)
 *     Depends on SPI (requires bus to be initialized).
 *     Auto-managed by consumer registration (not in boot_order).
 *
 * Consumer registration (spi_add_consumer / spi_remove_consumer) is the
 * single entry point for IMU, WAS, ACTUATOR to join/leave the SPI bus.
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
// Consumer Registry — shared between both modules
// ===================================================================

/// Bitmask of registered consumers (bits correspond to ModuleId enum values).
static uint8_t s_consumer_mask = 0;

static uint8_t popcount8(uint8_t v) {
    v = v - ((v >> 1) & 0x55);
    v = (v & 0x33) + ((v >> 2) & 0x33);
    return (v + (v >> 4)) & 0x0F;
}

static uint8_t consumerCount(void) {
    return popcount8(s_consumer_mask);
}

/// Find the ModuleId of the sole consumer.
/// Returns ModuleId::COUNT if not exactly one consumer.
static ModuleId soleConsumer(void) {
    if (consumerCount() != 1) return ModuleId::COUNT;
    for (uint8_t i = 0; i < static_cast<uint8_t>(ModuleId::SPI); i++) {
        if (s_consumer_mask & (1u << i)) {
            return static_cast<ModuleId>(i);
        }
    }
    return ModuleId::COUNT;
}

// ===================================================================
// Consumer Registration API (mod_spi.h)
// ===================================================================

void spi_add_consumer(ModuleId id) {
    if (id >= ModuleId::SPI) return;  // SPI/SPI_SHARED can't be consumers
    const uint8_t bit = 1u << static_cast<uint8_t>(id);
    if (s_consumer_mask & bit) return;  // already registered

    const uint8_t old_count = consumerCount();
    s_consumer_mask |= bit;
    const uint8_t new_count = consumerCount();

    if (old_count == 0 && new_count == 1) {
        // --- First consumer → activate SPI module (bus init, DIRECT mode) ---
        moduleSysActivate(ModuleId::SPI);
        LOGI("SPI", "bus INIT (consumer: %s, DIRECT mode)", moduleIdToName(id));
    } else if (old_count == 1 && new_count >= 2) {
        // --- 2nd consumer → activate SPI_SHARED module (mutex + arbitration) ---
        moduleSysActivate(ModuleId::SPI_SHARED);
        LOGI("SPI", "-> SHARED mode (%u consumers)", (unsigned)new_count);
    }
}

void spi_remove_consumer(ModuleId id) {
    if (id >= ModuleId::SPI) return;
    const uint8_t bit = 1u << static_cast<uint8_t>(id);
    if (!(s_consumer_mask & bit)) return;  // not registered

    const uint8_t old_count = consumerCount();
    s_consumer_mask &= ~bit;
    const uint8_t new_count = consumerCount();

    if (old_count >= 2 && new_count == 1) {
        // --- 2→1 consumers → deactivate SPI_SHARED, back to DIRECT ---
        moduleSysDeactivate(ModuleId::SPI_SHARED);
        ModuleId remaining = soleConsumer();
        LOGI("SPI", "-> DIRECT mode (1 consumer: %s)",
             remaining < ModuleId::SPI ? moduleIdToName(remaining) : "none");
    } else if (old_count == 1 && new_count == 0) {
        // --- Last consumer → deactivate SPI (bus deinit) ---
        moduleSysDeactivate(ModuleId::SPI);
        LOGI("SPI", "bus DEINIT (no consumers)");
    }
}

uint8_t spi_consumer_count(void) { return consumerCount(); }
bool spi_is_initialized(void) { return moduleSysIsActive(ModuleId::SPI); }
bool spi_is_direct_mode(void) { return moduleSysIsActive(ModuleId::SPI) && !moduleSysIsActive(ModuleId::SPI_SHARED); }
bool spi_is_shared_mode(void) { return moduleSysIsActive(ModuleId::SPI_SHARED); }

// ===================================================================
// SPI Module (basic — single-consumer / DIRECT mode)
// ===================================================================

static ModState s_spi_state;

static const ModuleId s_spi_deps[] = { ModuleId::COUNT };  // no dependencies

static bool mod_spi_is_enabled(void) {
    return feat::imu() || feat::ads() || feat::act();
}

static void mod_spi_activate(void) {
    hal_sensor_spi_init();
    hal_sensor_spi_set_multi_client(false);  // DIRECT mode
    s_spi_state = {};
    s_spi_state.detected = true;
    s_spi_state.quality_ok = true;
    s_spi_state.last_update_ms = hal_millis();
    LOGI("SPI", "activated (DIRECT mode)");
}

static void mod_spi_deactivate(void) {
    // Ensure SPI_SHARED is off first (should be handled by consumer removal order)
    if (moduleSysIsActive(ModuleId::SPI_SHARED)) {
        moduleSysDeactivate(ModuleId::SPI_SHARED);
    }
    hal_sensor_spi_deinit();
    s_spi_state = {};
    LOGI("SPI", "deactivated (bus released)");
}

static bool mod_spi_is_healthy(uint32_t) {
    // Healthy if active (bus initialized). No freshness timeout for infrastructure.
    return s_spi_state.detected;
}

static ModuleResult mod_spi_input(uint32_t) { return MOD_OK; }
static ModuleResult mod_spi_process(uint32_t) { return MOD_OK; }
static ModuleResult mod_spi_output(uint32_t) { return MOD_OK; }

static bool mod_spi_cfg_get(const char*, char*, size_t) { return false; }
static bool mod_spi_cfg_set(const char*, const char*) { return false; }
static bool mod_spi_cfg_apply(void) { return true; }
static bool mod_spi_cfg_save(void) { return true; }
static bool mod_spi_cfg_load(void) { return true; }

static bool mod_spi_cfg_show(void) {
    const uint8_t cnt = consumerCount();
    const char* mode = moduleSysIsActive(ModuleId::SPI_SHARED) ? "SHARED" :
                       moduleSysIsActive(ModuleId::SPI) ? "DIRECT" : "OFF";
    if (s_cli_out) {
        s_cli_out->printf("  SPI Bus:      %s\n", mode);
        s_cli_out->printf("  Consumers:    %u\n", (unsigned)cnt);
        s_cli_out->printf("  Bus:          %s\n", moduleSysIsActive(ModuleId::SPI) ? "INIT" : "DOWN");
        if (cnt > 0) {
            s_cli_out->print("  Devices:      ");
            for (uint8_t i = 0; i < static_cast<uint8_t>(ModuleId::SPI); i++) {
                if (s_consumer_mask & (1u << i)) {
                    s_cli_out->printf("%s ", moduleIdToName(static_cast<ModuleId>(i)));
                }
            }
            s_cli_out->println();
        }
    }
    return true;
}

static void mod_spi_diag_info(void) {
    if (!s_cli_out) return;
    if (!moduleSysIsActive(ModuleId::SPI)) {
        s_cli_out->printf("  Reason: bus not initialized\n");
        return;
    }
    if (moduleSysIsActive(ModuleId::SPI_SHARED)) {
        s_cli_out->printf("  Mode: SHARED (%u consumers)\n", (unsigned)consumerCount());
    } else {
        ModuleId c = soleConsumer();
        s_cli_out->printf("  Mode: DIRECT (consumer: %s)\n",
                         c < ModuleId::SPI ? moduleIdToName(c) : "none");
    }
}

static bool mod_spi_debug(void) {
    if (!s_cli_out) return moduleSysIsActive(ModuleId::SPI);

    const uint8_t cnt = consumerCount();
    const char* mode = !moduleSysIsActive(ModuleId::SPI) ? "OFF" :
                       moduleSysIsActive(ModuleId::SPI_SHARED) ? "SHARED" : "DIRECT";

    s_cli_out->println("=== SPI Debug ===");
    s_cli_out->printf("  Bus:        %s\n", moduleSysIsActive(ModuleId::SPI) ? "INITIALIZED" : "DOWN");
    s_cli_out->printf("  Mode:       %s\n", mode);
    s_cli_out->printf("  Consumers:  %u\n", (unsigned)cnt);

    s_cli_out->println("  Registered:");
    bool any = false;
    for (uint8_t i = 0; i < static_cast<uint8_t>(ModuleId::SPI); i++) {
        if (s_consumer_mask & (1u << i)) {
            ModuleId mid = static_cast<ModuleId>(i);
            const bool active = moduleSysIsActive(mid);
            s_cli_out->printf("    %-10s %s\n", moduleIdToName(mid), active ? "ACTIVE" : "INACTIVE(!)");
            any = true;
        }
    }
    if (!any) s_cli_out->println("    (none)");

    if (moduleSysIsActive(ModuleId::SPI) && !moduleSysIsActive(ModuleId::SPI_SHARED) && cnt == 1) {
        const char* cn = soleConsumer() < ModuleId::SPI ? moduleIdToName(soleConsumer()) : "?";
        s_cli_out->printf("  Direct:     single consumer (%s), no mutex\n", cn);
    }

    // Telemetry
    if (moduleSysIsActive(ModuleId::SPI)) {
        HalSpiTelemetry tm;
        hal_sensor_spi_get_telemetry(&tm);
        s_cli_out->printf("  Window:     %lu ms\n", (unsigned long)tm.window_ms);
        s_cli_out->printf("  Busy:       %lu us (%.1f%%)\n",
                          (unsigned long)tm.bus_busy_us, (double)tm.bus_utilization_pct);
        s_cli_out->printf("  Transfers:  %lu\n", (unsigned long)tm.bus_transactions);
        s_cli_out->printf("  Switches:   %lu\n", (unsigned long)tm.client_switches);
        s_cli_out->printf("  Deadline miss: IMU=%lu WAS=%lu\n",
                          (unsigned long)tm.imu_deadline_miss,
                          (unsigned long)tm.was_deadline_miss);
    }

    return moduleSysIsActive(ModuleId::SPI);
}

const ModuleOps2 mod_spi_ops = {
    "SPI", ModuleId::SPI,
    mod_spi_is_enabled, mod_spi_activate, mod_spi_deactivate, mod_spi_is_healthy,
    mod_spi_input, mod_spi_process, mod_spi_output,
    nullptr,  // cfg_keys
    mod_spi_cfg_get, mod_spi_cfg_set, mod_spi_cfg_apply,
    mod_spi_cfg_save, mod_spi_cfg_load, mod_spi_cfg_show,
    mod_spi_diag_info, mod_spi_debug,
    s_spi_deps
};

// ===================================================================
// SPI_SHARED Module (multi-client / SHARED mode)
// ===================================================================

static ModState s_spi_shared_state;

static const ModuleId s_spi_shared_deps[] = {
    ModuleId::SPI,
    ModuleId::COUNT   // depends on SPI bus being initialized
};

static bool mod_spi_shared_is_enabled(void) {
    return feat::imu() || feat::ads() || feat::act();
}

static void mod_spi_shared_activate(void) {
    hal_sensor_spi_set_multi_client(true);  // SHARED mode
    s_spi_shared_state = {};
    s_spi_shared_state.detected = true;
    s_spi_shared_state.quality_ok = true;
    s_spi_shared_state.last_update_ms = hal_millis();
    LOGI("SPI", "SHARED mode enabled (mutex + CS arbitration)");
}

static void mod_spi_shared_deactivate(void) {
    hal_sensor_spi_set_multi_client(false);  // back to DIRECT
    s_spi_shared_state = {};
    LOGI("SPI", "SHARED mode disabled (back to DIRECT)");
}

static bool mod_spi_shared_is_healthy(uint32_t) {
    // Healthy if SPI module is active (our dependency).
    return moduleSysIsActive(ModuleId::SPI) && s_spi_shared_state.detected;
}

static ModuleResult mod_spi_shared_input(uint32_t) { return MOD_OK; }
static ModuleResult mod_spi_shared_process(uint32_t) { return MOD_OK; }
static ModuleResult mod_spi_shared_output(uint32_t) { return MOD_OK; }

static bool mod_spi_shared_cfg_get(const char*, char*, size_t) { return false; }
static bool mod_spi_shared_cfg_set(const char*, const char*) { return false; }
static bool mod_spi_shared_cfg_apply(void) { return true; }
static bool mod_spi_shared_cfg_save(void) { return true; }
static bool mod_spi_shared_cfg_load(void) { return true; }

static bool mod_spi_shared_cfg_show(void) {
    if (s_cli_out) {
        s_cli_out->printf("  SHARED Mode:  %s\n", moduleSysIsActive(ModuleId::SPI_SHARED) ? "ACTIVE" : "OFF");
        s_cli_out->printf("  Consumers:    %u\n", (unsigned)consumerCount());
        s_cli_out->printf("  Mutex:        %s\n", moduleSysIsActive(ModuleId::SPI_SHARED) ? "ENABLED" : "DISABLED");
    }
    return true;
}

static void mod_spi_shared_diag_info(void) {
    if (!s_cli_out) return;
    if (!moduleSysIsActive(ModuleId::SPI_SHARED)) {
        s_cli_out->printf("  Reason: not needed (only %u consumer)\n", (unsigned)consumerCount());
    } else {
        s_cli_out->printf("  Reason: active (%u consumers, mutex arbitration)\n", (unsigned)consumerCount());
    }
}

static bool mod_spi_shared_debug(void) {
    if (!s_cli_out) return moduleSysIsActive(ModuleId::SPI_SHARED);

    s_cli_out->println("=== SPI_SHARED Debug ===");
    s_cli_out->printf("  Active:      %s\n", moduleSysIsActive(ModuleId::SPI_SHARED) ? "YES" : "NO");
    s_cli_out->printf("  Consumers:   %u\n", (unsigned)consumerCount());
    s_cli_out->printf("  Mutex:       %s\n", moduleSysIsActive(ModuleId::SPI_SHARED) ? "ENABLED" : "DISABLED");

    if (moduleSysIsActive(ModuleId::SPI_SHARED)) {
        HalSpiTelemetry tm;
        hal_sensor_spi_get_telemetry(&tm);
        s_cli_out->printf("  Switches:   %lu (WAS->IMU=%lu IMU->WAS=%lu)\n",
                          (unsigned long)tm.client_switches,
                          (unsigned long)tm.was_to_imu_switches,
                          (unsigned long)tm.imu_to_was_switches);
        s_cli_out->printf("  Utilization: %.1f%%\n", (double)tm.bus_utilization_pct);
    }

    return moduleSysIsActive(ModuleId::SPI_SHARED);
}

const ModuleOps2 mod_spi_shared_ops = {
    "SPI_SHARED", ModuleId::SPI_SHARED,
    mod_spi_shared_is_enabled, mod_spi_shared_activate, mod_spi_shared_deactivate, mod_spi_shared_is_healthy,
    mod_spi_shared_input, mod_spi_shared_process, mod_spi_shared_output,
    nullptr,  // cfg_keys
    mod_spi_shared_cfg_get, mod_spi_shared_cfg_set, mod_spi_shared_cfg_apply,
    mod_spi_shared_cfg_save, mod_spi_shared_cfg_load, mod_spi_shared_cfg_show,
    mod_spi_shared_diag_info, mod_spi_shared_debug,
    s_spi_shared_deps
};
