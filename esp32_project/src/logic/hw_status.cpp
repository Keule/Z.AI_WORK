/**
 * @file hw_status.cpp
 * @brief Hardware status monitoring implementation.
 *
 * Monitors all subsystems and sends PGN 0xDD hardware messages to AgIO.
 * Messages are rate-limited to avoid flooding the network.
 *
 * Pure C++ logic – uses HAL only for timing and network send.
 */

#include "hw_status.h"
#include "pgn_codec.h"
#include "pgn_types.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_HWS
#include "esp_log.h"
#include "log_ext.h"

#include <cstdarg>
#include <cstdio>

// ===================================================================
// Configuration
// ===================================================================

static const HwRateLimitPolicy HW_RATE_POLICY = {
    500,    // startup_min_interval_ms
    2000,   // runtime_min_interval_ms
    3000,   // runtime_error_debounce_ms
    10000   // runtime_resend_interval_ms
};

// ===================================================================
// State
// ===================================================================

/// Per-subsystem status
static HwSubsysStatus s_subsys[HW_COUNT];

/// Timestamp of last hardware message sent (rate limiting, path-aware)
static uint32_t s_last_startup_msg_sent_ms = 0;
static uint32_t s_last_runtime_msg_sent_ms = 0;

// ===================================================================
// Human-readable subsystem names (for HW messages)
// ===================================================================
static const char* const s_subsys_names[HW_COUNT] = {
    "Ethernet",     // HW_ETH
    "IMU",          // HW_IMU
    "Steer Angle",  // HW_WAS
    "Actuator",     // HW_ACTUATOR
    "Safety",       // HW_SAFETY
    "GNSS"          // HW_GNSS — TASK-025
};

// ===================================================================
// Init
// ===================================================================
void hwStatusInit(void) {
    for (uint8_t i = 0; i < HW_COUNT; i++) {
        s_subsys[i].error      = false;
        s_subsys[i].severity   = HW_SEV_OK;
        s_subsys[i].first_seen = 0;
        s_subsys[i].last_sent  = 0;
    }
    s_last_startup_msg_sent_ms = 0;
    s_last_runtime_msg_sent_ms = 0;
    LOGI("HWS", "monitoring initialised (%u subsystems)", (unsigned)HW_COUNT);
}

// ===================================================================
// Set / Clear / Query flags
// ===================================================================
void hwStatusSetFlag(HwSubsystem id, HwSeverity severity) {
    if (id >= HW_COUNT) return;
    // HW_SEV_OK means "no error" — clear the flag instead of setting it.
    // This prevents callers from accidentally marking a subsystem as faulty
    // when they intend to signal that everything is OK (see TASK-032).
    if (severity == HW_SEV_OK) {
        hwStatusClearFlag(id);
        return;
    }
    if (!s_subsys[id].error) {
        s_subsys[id].error = true;
        s_subsys[id].severity = severity;
        s_subsys[id].first_seen = hal_millis();
        LOGE("HWS", "ERROR flagged: %s (sev=%u)",
                s_subsys_names[id], (unsigned)severity);
    }
}

void hwStatusClearFlag(HwSubsystem id) {
    if (id >= HW_COUNT) return;
    if (s_subsys[id].error) {
        LOGI("HWS", "CLEARED: %s", s_subsys_names[id]);
    }
    s_subsys[id].error = false;
    s_subsys[id].severity = HW_SEV_OK;
}

bool hwStatusHasError(HwSubsystem id) {
    if (id >= HW_COUNT) return false;
    return s_subsys[id].error;
}

uint8_t hwStatusErrorCount(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < HW_COUNT; i++) {
        if (s_subsys[i].error) count++;
    }
    return count;
}

// ===================================================================
// Internal: send a formatted hardware message
// ===================================================================
static bool sendHwMessage(uint8_t src, uint8_t color, uint8_t duration,
                          const char* text, HwErrorClass err_class) {
    uint32_t now = hal_millis();

    uint32_t* last_sent_ms = (err_class == HW_ERR_CLASS_STARTUP)
                             ? &s_last_startup_msg_sent_ms
                             : &s_last_runtime_msg_sent_ms;
    const uint32_t min_interval_ms =
        (err_class == HW_ERR_CLASS_STARTUP)
            ? HW_RATE_POLICY.startup_min_interval_ms
            : HW_RATE_POLICY.runtime_min_interval_ms;

    // Startup path may still need fallback logs even without network.
    if (err_class == HW_ERR_CLASS_STARTUP && !hal_net_is_connected()) {
        LOGW("HWS", "[startup-serial] %s", text);
        return false;
    }

    // Runtime path: do nothing if no network transport is available.
    if (err_class == HW_ERR_CLASS_RUNTIME && !hal_net_is_connected()) {
        return false;
    }

    // Rate limit: minimum interval between messages for this path
    if (now - *last_sent_ms < min_interval_ms) return false;

    uint8_t tx_buf[aog_frame::MAX_FRAME];
    size_t len = pgnEncodeHardwareMessage(tx_buf, sizeof(tx_buf),
                                           src, duration, color, text);
    if (len > 0) {
        hal_net_send(tx_buf, len, aog_port::AGIO_SEND);
        *last_sent_ms = now;
        LOGI("HWS", "sent msg class=%u color=%u dur=%u \"%s\"",
             (unsigned)err_class, (unsigned)color, (unsigned)duration, text);
        return true;
    }
    return false;
}

// ===================================================================
// Send a one-off hardware message (varargs)
// ===================================================================
void hwStatusSendMessage(uint8_t src, HwSeverity severity, uint8_t duration,
                          const char* fmt, ...) {
    char text[aog_frame::HWMSG_MAX_TEXT + 1];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    (void)sendHwMessage(src, severity, duration, text, HW_ERR_CLASS_RUNTIME);
}

HwSeverity hwStatusPriorityToSeverity(HwErrorPriority prio) {
    switch (prio) {
        case HW_ERR_PRIO_P1: return HW_SEV_ERROR;
        case HW_ERR_PRIO_P2: return HW_SEV_WARNING;
        case HW_ERR_PRIO_P3: return HW_SEV_INFO;
        default: return HW_SEV_ERROR;
    }
}

const HwRateLimitPolicy* hwStatusPolicy(void) {
    return &HW_RATE_POLICY;
}

void hwStatusSendClassifiedMessage(uint8_t src,
                                   HwErrorClass err_class,
                                   HwErrorPriority prio,
                                   uint8_t duration,
                                   const char* fmt, ...) {
    char text[aog_frame::HWMSG_MAX_TEXT + 1];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    (void)sendHwMessage(src, hwStatusPriorityToSeverity(prio), duration, text, err_class);
}

// ===================================================================
// Periodic update – check all subsystems, send messages
// ===================================================================
uint8_t hwStatusUpdate(bool connected,
                        bool safety_ok,
                        bool steer_angle_valid,
                        bool imu_valid,
                        bool ntrip_active,
                        bool imu_monitored,
                        bool steer_monitored,
                        bool safety_monitored) {
    uint32_t now = hal_millis();

    // --- Update subsystem flags based on actual status ---

    // Ethernet
    if (!connected) {
        hwStatusSetFlag(HW_ETH, HW_SEV_ERROR);
    } else {
        hwStatusClearFlag(HW_ETH);
    }

    // IMU
    if (!imu_monitored) {
        hwStatusClearFlag(HW_IMU);
    } else if (!imu_valid) {
        hwStatusSetFlag(HW_IMU, HW_SEV_WARNING);
    } else {
        hwStatusClearFlag(HW_IMU);
    }

    // Steer Angle Sensor
    if (!steer_monitored) {
        hwStatusClearFlag(HW_WAS);
    } else if (!steer_angle_valid) {
        hwStatusSetFlag(HW_WAS, HW_SEV_WARNING);
    } else {
        hwStatusClearFlag(HW_WAS);
    }

    // Safety circuit
    if (!safety_monitored) {
        hwStatusClearFlag(HW_SAFETY);
    } else if (!safety_ok) {
        hwStatusSetFlag(HW_SAFETY, HW_SEV_ERROR);
    } else {
        hwStatusClearFlag(HW_SAFETY);
    }

    // GNSS/NTRIP subsystem — clear flag if NTRIP module is not active.
    // When NTRIP is active, hwStatusSetFlag(HW_GNSS, ...) is called from
    // ntripTick() in ntrip.cpp (TASK-025). We only need to clear the flag
    // here when the module is inactive.
    if (!ntrip_active) {
        hwStatusClearFlag(HW_GNSS);
    }

    // --- Send hardware messages for active errors ---

    for (uint8_t i = 0; i < HW_COUNT; i++) {
        if (!s_subsys[i].error) continue;

        // Debounce: wait before first message
        if (now - s_subsys[i].first_seen < HW_RATE_POLICY.runtime_error_debounce_ms) continue;

        // Resend interval for persistent errors
        if (s_subsys[i].last_sent > 0 &&
            now - s_subsys[i].last_sent < HW_RATE_POLICY.runtime_resend_interval_ms) {
            continue;
        }

        // Build message text
        char text[aog_frame::HWMSG_MAX_TEXT + 1];
        const char* name = s_subsys_names[i];
        const char* sev_text = (s_subsys[i].severity == HW_SEV_ERROR) ? "ERR" : "WARN";

        std::snprintf(text, sizeof(text), "%s %s: %s", sev_text, name,
                      (s_subsys[i].severity == HW_SEV_ERROR) ? "Failure" : "Not Available");

        // Send from steer module source
        if (sendHwMessage(aog_src::STEER, s_subsys[i].severity,
                          aog_hwmsg::DURATION_PERSIST, text, HW_ERR_CLASS_RUNTIME)) {
            s_subsys[i].last_sent = now;
        }
    }

    // --- If all errors cleared and we had errors before, send OK message ---
    // (Only once, when transitioning from error state to OK)
    static bool s_had_errors = false;
    uint8_t err_count = hwStatusErrorCount();

    if (err_count > 0) {
        s_had_errors = true;
    } else if (s_had_errors) {
        // All errors cleared
        s_had_errors = false;
        // Send a single "all OK" message (green)
        (void)sendHwMessage(aog_src::STEER, HW_SEV_OK, 3, "All systems OK",
                            HW_ERR_CLASS_RUNTIME);
    }

    return err_count;
}
