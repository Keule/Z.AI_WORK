/**
 * @file mod_logging.cpp
 * @brief Module LOGGING — SD-Card data logger implementation.
 *
 * Pipeline:
 *   input()   — No-Op (sensors read elsewhere)
 *   process() — No-Op
 *   output()  — calls sdLoggerRecord() to buffer one sample
 *
 * The maintenance task (draining the ring buffer to SD) is started by
 * sdLoggerMaintInit() and runs independently of the pipeline.
 *
 * Logging has no freshness concept — it is asynchronous and event-driven
 * (hardware switch + ring buffer).  Health is simply: detected && no error.
 */

#include "mod_logging.h"
#include "sd_logger.h"
#include "global_state.h"
#include "hal/hal.h"

#include <cstdio>
#include <cstring>

#include "cli.h"

extern Stream* s_cli_out;

// ===================================================================
// Error codes (module-specific)
// ===================================================================

enum LoggingError : uint32_t {
    LOG_ERR_OK             = 0,
    LOG_ERR_SD_NOT_PRESENT = 1,
    LOG_ERR_WRITE_ERROR    = 2,
    LOG_ERR_BUFFER_OVERFLOW = 3,
};

// ===================================================================
// Dependencies — nullptr (logging has no module dependencies)
// ===================================================================

// ===================================================================
// Module state
// ===================================================================

static ModState s_state;

/// Whether SD card was detected at activate time.
static bool s_sd_detected = false;

// ===================================================================
// Configuration parameters
// ===================================================================

/// Logging subsample interval [ms] — not directly used by sd_logger
/// (it uses a call-counter divider), kept for API compatibility.
static uint32_t cfg_interval_ms = 100;

/// Whether logging should be active by default.
static bool cfg_default_active = true;

/// GPIO pin for the logging hardware switch.
static int cfg_gpio_pin = 47;   // GPIO 47, active LOW

// ===================================================================
// Lifecycle: is_enabled — logging is always compiled in
// ===================================================================

static bool mod_logging_is_enabled(void) {
    (void)0;
    return true;
}

// ===================================================================
// Lifecycle: activate
// ===================================================================

static void mod_logging_activate(void) {
    s_sd_detected = hal_sd_card_present();

    if (s_sd_detected) {
        // Start the maintenance task + PSRAM ring buffer.
        sdLoggerMaintInit();
        s_state.detected = true;
        s_state.error_code = LOG_ERR_OK;
        hal_log("LOGGING: activated (SD card detected)");
    } else {
        s_state.detected = false;
        s_state.error_code = LOG_ERR_SD_NOT_PRESENT;
        hal_log("LOGGING: activated but SD card NOT detected (err=%u)",
                (unsigned)LOG_ERR_SD_NOT_PRESENT);
    }

    s_state.last_update_ms = hal_millis();
}

// ===================================================================
// Lifecycle: deactivate — No-Op
// ===================================================================

static void mod_logging_deactivate(void) {
    // SD logger flushes on its own via the maintenance task.
    // No explicit shutdown action required here.
    s_state.detected = false;
    s_state.quality_ok = false;
    s_state.error_code = 0;
    s_state.last_update_ms = 0;
    s_sd_detected = false;
    hal_log("LOGGING: deactivated");
}

// ===================================================================
// Lifecycle: is_healthy — check detected + no error (no freshness)
// ===================================================================

static bool mod_logging_is_healthy(uint32_t now_ms) {
    (void)now_ms;

    // 1. Detected (SD card present)
    if (!s_state.detected) return false;

    // 2. No error
    if (s_state.error_code != 0) return false;

    // No freshness check — logging is async / event-driven.
    // Quality is always OK if we're this far.
    s_state.quality_ok = true;
    return true;
}

// ===================================================================
// Pipeline: input — No-Op
// ===================================================================

static ModuleResult mod_logging_input(uint32_t now_ms) {
    (void)now_ms;
    return MOD_OK;
}

// ===================================================================
// Pipeline: process — No-Op
// ===================================================================

static ModuleResult mod_logging_process(uint32_t now_ms) {
    (void)now_ms;
    return MOD_OK;
}

// ===================================================================
// Pipeline: output — buffer one log record
// ===================================================================

static ModuleResult mod_logging_output(uint32_t now_ms) {
    (void)now_ms;

    if (!s_sd_detected) {
        return { true, LOG_ERR_SD_NOT_PRESENT };
    }

    // sdLoggerRecord() is very fast (~1 µs) — subsamples internally.
    // It checks the hardware switch before buffering.
    sdLoggerRecord();

    // Check for buffer overflow (diagnostic only).
    // NOTE: overflow count is internal to sd_logger.cpp and not directly
    // queryable from here.  If we add a getter later we can set the error.
    s_state.last_update_ms = hal_millis();

    return MOD_OK;
}

// ===================================================================
// Configuration: cfg_get
// ===================================================================

static bool mod_logging_cfg_get(const char* key, char* buf, size_t len) {
    if (!key || !buf || len == 0) return false;

    if (std::strcmp(key, "interval_ms") == 0) {
        std::snprintf(buf, len, "%lu", (unsigned long)cfg_interval_ms);
        return true;
    }
    if (std::strcmp(key, "default_active") == 0) {
        std::snprintf(buf, len, "%s", cfg_default_active ? "true" : "false");
        return true;
    }
    if (std::strcmp(key, "gpio_pin") == 0) {
        std::snprintf(buf, len, "%d", cfg_gpio_pin);
        return true;
    }
    return false;
}

// ===================================================================
// Configuration: cfg_set
// ===================================================================

static bool mod_logging_cfg_set(const char* key, const char* val) {
    if (!key || !val) return false;

    if (std::strcmp(key, "interval_ms") == 0) {
        unsigned long v = 0;
        if (std::sscanf(val, "%lu", &v) != 1) return false;
        cfg_interval_ms = static_cast<uint32_t>(v);
        return true;
    }
    if (std::strcmp(key, "default_active") == 0) {
        if (std::strcmp(val, "true") == 0 || std::strcmp(val, "1") == 0) {
            cfg_default_active = true;
        } else if (std::strcmp(val, "false") == 0 || std::strcmp(val, "0") == 0) {
            cfg_default_active = false;
        } else {
            return false;
        }
        return true;
    }
    if (std::strcmp(key, "gpio_pin") == 0) {
        int v = 0;
        if (std::sscanf(val, "%d", &v) != 1) return false;
        cfg_gpio_pin = v;
        return true;
    }
    return false;
}

// ===================================================================
// Configuration: cfg_apply
// ===================================================================

static bool mod_logging_cfg_apply(void) {
    hal_log("LOGGING: config applied (interval=%lu ms, gpio=%d, default=%s)",
            (unsigned long)cfg_interval_ms, cfg_gpio_pin,
            cfg_default_active ? "true" : "false");
    return true;
}

// ===================================================================
// Configuration: cfg_save / cfg_load
// ===================================================================

static bool mod_logging_cfg_save(void) {
    // TODO: persist to NVS
    hal_log("LOGGING: cfg_save (NVS not yet implemented)");
    return true;
}

static bool mod_logging_cfg_load(void) {
    // TODO: load from NVS; for now use compile-time defaults
    hal_log("LOGGING: cfg_load (using defaults)");
    return true;
}

// ===================================================================
// Configuration: cfg_show
// ===================================================================

static bool mod_logging_cfg_show(void) {
    hal_log("LOGGING config:");
    hal_log("  interval_ms    = %lu", (unsigned long)cfg_interval_ms);
    hal_log("  default_active = %s", cfg_default_active ? "true" : "false");
    hal_log("  gpio_pin       = %d", cfg_gpio_pin);
    hal_log("  sd_detected    = %s", s_sd_detected ? "yes" : "no");
    hal_log("  records_flushed= %lu", (unsigned long)sdLoggerGetRecordsFlushed());
    hal_log("  buffer_count   = %lu", (unsigned long)sdLoggerGetBufferCount());
    hal_log("  psram_buffer   = %s", sdLoggerPsramBufferActive() ? "yes" : "no");
    return true;
}

// ===================================================================
// Diag info
// ===================================================================

static void mod_logging_diag_info(void) {
    if (!s_sd_detected) {
        s_cli_out->printf("  Reason:    SD card not detected (error 1)\n");
    } else if (s_state.error_code != 0) {
        s_cli_out->printf("  Reason:    error code %lu\n", (unsigned long)s_state.error_code);
    } else {
        s_cli_out->printf("  Reason:    OK — SD active, flushed=%lu records\n",
            (unsigned long)sdLoggerGetRecordsFlushed());
    }
}

// ===================================================================
// Debug
// ===================================================================

static bool mod_logging_debug(void) {
    s_cli_out->printf("LOGGING debug:\n");
    s_cli_out->printf("  detected       = %s\n", s_state.detected ? "yes" : "no");
    s_cli_out->printf("  quality_ok     = %s\n", s_state.quality_ok ? "yes" : "no");
    s_cli_out->printf("  error_code     = %lu\n", (unsigned long)s_state.error_code);
    s_cli_out->printf("  last_update    = %lu ms\n", (unsigned long)s_state.last_update_ms);
    s_cli_out->printf("  sd_detected    = %s\n", s_sd_detected ? "yes" : "no");
    s_cli_out->printf("  is_active      = %s\n", sdLoggerIsActive() ? "yes" : "no");
    s_cli_out->printf("  records_flushed= %lu\n", (unsigned long)sdLoggerGetRecordsFlushed());
    s_cli_out->printf("  buffer_count   = %lu\n", (unsigned long)sdLoggerGetBufferCount());
    s_cli_out->printf("  psram_buffer   = %s\n", sdLoggerPsramBufferActive() ? "yes" : "no");
    return true;
}

// ===================================================================
// Public helpers
// ===================================================================

bool mod_logging_sd_detected(void) {
    return s_sd_detected;
}

uint32_t mod_logging_records_flushed(void) {
    return sdLoggerGetRecordsFlushed();
}

// ===================================================================
// Config key definitions
// ===================================================================
static const CfgKeyDef s_logging_keys[] = {
    {"interval_ms",    "Logging subsample interval (ms)"},
    {"default_active", "Logging active by default (true/false)"},
    {"gpio_pin",       "GPIO pin for hardware switch"},
    {nullptr, nullptr}  // sentinel
};

static const CfgKeyDef* mod_logging_cfg_keys(void) { return s_logging_keys; }

// ===================================================================
// Ops table — const, ModuleOps2 (15 function pointers)
// ===================================================================

const ModuleOps2 mod_logging_ops = {
    .name        = "LOGGING",
    .id          = ModuleId::LOGGING,

    .is_enabled  = mod_logging_is_enabled,
    .activate    = mod_logging_activate,
    .deactivate  = mod_logging_deactivate,
    .is_healthy  = mod_logging_is_healthy,

    .input       = mod_logging_input,
    .process     = mod_logging_process,
    .output      = mod_logging_output,

    .cfg_keys    = mod_logging_cfg_keys,
    .cfg_get     = mod_logging_cfg_get,
    .cfg_set     = mod_logging_cfg_set,
    .cfg_apply   = mod_logging_cfg_apply,
    .cfg_save    = mod_logging_cfg_save,
    .cfg_load    = mod_logging_cfg_load,
    .cfg_show    = mod_logging_cfg_show,

    .diag_info   = mod_logging_diag_info,
    .debug       = mod_logging_debug,

    .deps        = nullptr,   // no module dependencies
};
