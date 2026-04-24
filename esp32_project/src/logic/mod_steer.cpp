/**
 * @file mod_steer.cpp
 * @brief Module STEER — PID steering controller implementation.
 *
 * Pipeline phases:
 *   input()   — snapshot g_nav state (IMU, WAS, safety, switches)
 *   process() — PID computation, safety/watchdog checks
 *   output()  — write actuator command via mod_actuator
 *
 * Migrated from control.cpp. PID math (pidInit / pidCompute / pidReset)
 * is now fully inlined here — no dependency on old control.h.
 */

#include "mod_steer.h"
#include "mod_actuator.h"
#include "dependency_policy.h"
#include "global_state.h"
#include "features.h"
#include "hal/hal.h"

#include <cmath>
#include <cstdio>
#include <cstring>

// ===================================================================
// PID Controller — inlined from control.cpp
// ===================================================================

/// PID controller state.
struct PidState {
    float kp;               // Proportional gain
    float ki;               // Integral gain
    float kd;               // Derivative gain
    float integral;         // Accumulated integral term
    float prev_error;       // Previous error for derivative
    float output_min;       // Minimum output
    float output_max;       // Maximum output
    uint32_t last_update_ms;
    bool   first_update;
};

static void pidInit(PidState* pid, float kp, float ki, float kd,
                    float out_min, float out_max) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->output_min = out_min;
    pid->output_max = out_max;
    pid->last_update_ms = hal_millis();
    pid->first_update = true;
}

static void pidReset(PidState* pid) {
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->first_update = true;
}

static float pidCompute(PidState* pid, float error, uint32_t dt_ms) {
    if (dt_ms == 0) dt_ms = 5;  // safety: assume 5 ms (= 200 Hz)

    float dt_s = dt_ms * 0.001f;

    // Proportional
    float p_term = pid->kp * error;

    // Integral with anti-windup
    pid->integral += error * dt_s;
    float i_term = pid->ki * pid->integral;

    // Clamp integral to prevent windup
    if (i_term > pid->output_max) {
        i_term = pid->output_max;
        pid->integral = i_term / pid->ki;
    } else if (i_term < pid->output_min) {
        i_term = pid->output_min;
        pid->integral = i_term / pid->ki;
    }

    // Derivative (on error, not measurement)
    float d_term = 0.0f;
    if (!pid->first_update && dt_s > 0.0f) {
        float derivative = (error - pid->prev_error) / dt_s;
        d_term = pid->kd * derivative;
    }
    pid->first_update = false;
    pid->prev_error = error;

    // Sum
    float output = p_term + i_term + d_term;

    // Clamp output
    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;

    return output;
}

// ===================================================================
// Constants
// ===================================================================

/// Minimum GPS speed [km/h] to allow auto-steering.
static constexpr float MIN_STEER_SPEED_KMH = 0.1f;

/// Freshness timeout for steer module [ms].
static constexpr uint32_t STEER_FRESHNESS_MS = 500;

/// Error codes (module-specific).
enum SteerError : uint32_t {
    STEER_ERR_OK              = 0,
    STEER_ERR_PID_COMPUTE     = 1,
    STEER_ERR_SAFETY_KICK     = 2,
    STEER_ERR_WATCHDOG        = 3,
};

// ===================================================================
// Dependencies
// ===================================================================

/// STEER requires IMU, WAS, ACTUATOR, and SAFETY modules.
static const ModuleId s_deps[] = {
    ModuleId::IMU,
    ModuleId::WAS,
    ModuleId::ACTUATOR,
    ModuleId::SAFETY,
    ModuleId::COUNT   // sentinel
};

// ===================================================================
// Module state
// ===================================================================

static ModState s_state;

/// Module-local PID instance (migrated from control.cpp s_steer_pid).
static PidState s_pid;

/// Manual actuator mode (disables PID output).
static bool s_manual_actuator_mode = false;

/// Latest PID output command (for output phase + diagnostics).
static uint16_t s_actuator_cmd = 0;

/// Snapshot of g_nav taken during input() — consumed by process().
struct SteerSnapshot {
    float    heading_deg         = 0.0f;
    float    yaw_rate_dps        = 0.0f;
    float    roll_deg            = 0.0f;
    float    steer_angle_deg     = 0.0f;
    int16_t  steer_angle_raw     = 0;
    bool     steer_quality       = false;
    bool     imu_quality         = false;
    bool     safety_ok           = false;
    bool     watchdog_triggered  = false;
    float    speed_kmh           = 0.0f;
    bool     work_switch         = false;
    bool     steer_switch        = false;
    float    desired_angle_deg   = 0.0f;
    uint32_t was_timestamp_ms    = 0;
    uint32_t imu_timestamp_ms    = 0;
    uint32_t snapshot_ms         = 0;
};

static SteerSnapshot s_snap;

// ===================================================================
// Configuration parameters (runtime-tunable, mirrored into s_pid)
// ===================================================================

static float  cfg_kp        = 1.0f;
static float  cfg_ki        = 0.0f;
static float  cfg_kd        = 0.01f;
static float  cfg_out_min   = 0.0f;
static float  cfg_out_max   = 65535.0f;

// ===================================================================
// Lifecycle: is_enabled
// ===================================================================

static bool mod_steer_is_enabled(void) {
    return feat::act() && feat::safety();
}

// ===================================================================
// Lifecycle: activate
// ===================================================================

static void mod_steer_activate(void) {
    // Initialise our local PID with current config values.
    pidInit(&s_pid, cfg_kp, cfg_ki, cfg_kd, cfg_out_min, cfg_out_max);

    s_state.detected = true;
    s_state.error_code = 0;
    s_state.last_update_ms = hal_millis();
    s_actuator_cmd = 0;
    s_snap = {};

    hal_log("STEER: activated (PID Kp=%.2f Ki=%.3f Kd=%.3f)",
            cfg_kp, cfg_ki, cfg_kd);
}

// ===================================================================
// Lifecycle: deactivate
// ===================================================================

static void mod_steer_deactivate(void) {
    // Reset module-local PID state to prevent stale integral on next activation.
    pidReset(&s_pid);

    // Drive actuator to neutral (0).
    if (feat::act()) {
        mod_actuator_set_cmd(0);
    }

    s_actuator_cmd = 0;
    s_state.detected = false;
    s_state.quality_ok = false;
    s_state.error_code = 0;
    s_state.last_update_ms = 0;
    hal_log("STEER: deactivated");
}

// ===================================================================
// Lifecycle: is_healthy — 4-condition check
// ===================================================================

static bool mod_steer_is_healthy(uint32_t now_ms) {
    // 1. Detected
    if (!s_state.detected) return false;

    // 2. Quality OK — steer angle and IMU both within freshness window
    const bool steer_fresh = dep_policy::isSteerAngleInputValid(
        now_ms, s_snap.was_timestamp_ms, s_snap.steer_quality);
    const bool imu_fresh = dep_policy::isImuInputValid(
        now_ms, s_snap.imu_timestamp_ms, s_snap.imu_quality);
    s_state.quality_ok = steer_fresh && imu_fresh;
    if (!s_state.quality_ok) return false;

    // 3. No error code
    if (s_state.error_code != 0) return false;

    // 4. Freshness — last pipeline update within timeout
    if ((now_ms - s_state.last_update_ms) > STEER_FRESHNESS_MS) return false;

    return true;
}

// ===================================================================
// Pipeline: input — snapshot g_nav
// ===================================================================

static ModuleResult mod_steer_input(uint32_t now_ms) {
    // The pipeline calls input() for all active modules in order.
    // IMU, WAS, and SAFETY have already updated g_nav before this runs.
    SteerSnapshot snap;
    snap.snapshot_ms = now_ms;

    {
        StateLock lock;
        snap.heading_deg       = g_nav.imu.heading_deg;
        snap.yaw_rate_dps      = g_nav.imu.yaw_rate_dps;
        snap.roll_deg          = g_nav.imu.roll_deg;
        snap.imu_timestamp_ms  = g_nav.imu.imu_timestamp_ms;
        snap.imu_quality       = g_nav.imu.imu_quality_ok;

        snap.steer_angle_deg   = g_nav.steer.steer_angle_deg;
        snap.steer_angle_raw   = g_nav.steer.steer_angle_raw;
        snap.was_timestamp_ms  = g_nav.steer.steer_angle_timestamp_ms;
        snap.steer_quality     = g_nav.steer.steer_angle_quality_ok;

        snap.safety_ok         = g_nav.safety.safety_ok;
        snap.watchdog_triggered = g_nav.safety.watchdog_triggered;

        snap.speed_kmh         = g_nav.sw.gps_speed_kmh;
        snap.work_switch       = g_nav.sw.work_switch;
        snap.steer_switch      = g_nav.sw.steer_switch;
        snap.desired_angle_deg = g_nav.sw.desiredSteerAngleDeg;
    }

    s_snap = snap;
    return MOD_OK;
}

// ===================================================================
// Pipeline: process — PID computation
// ===================================================================

static ModuleResult mod_steer_process(uint32_t now_ms) {
    // --- Safety check ---
    if (!s_snap.safety_ok) {
        s_state.error_code = STEER_ERR_SAFETY_KICK;
        s_actuator_cmd = 0;
        return { true, STEER_ERR_SAFETY_KICK };
    }

    // --- Watchdog check ---
    if (s_snap.watchdog_triggered) {
        s_state.error_code = STEER_ERR_WATCHDOG;
        s_actuator_cmd = 0;
        return { true, STEER_ERR_WATCHDOG };
    }

    // --- Check if steering is possible ---
    const bool steer_possible =
        moduleSysIsActive(ModuleId::ACTUATOR) &&
        moduleSysIsActive(ModuleId::WAS) &&
        moduleSysIsActive(ModuleId::IMU) &&
        !s_manual_actuator_mode &&
        s_snap.work_switch &&
        s_snap.steer_switch &&
        s_snap.speed_kmh >= MIN_STEER_SPEED_KMH;

    if (!steer_possible) {
        s_actuator_cmd = 0;
        s_state.error_code = STEER_ERR_OK;
        return MOD_OK;
    }

    // --- PID computation ---
    float error = s_snap.desired_angle_deg - s_snap.steer_angle_deg;

    // Wrap error to [-180, +180]
    while (error > 180.0f)  error -= 360.0f;
    while (error < -180.0f) error += 360.0f;

    // Compute timestep
    uint32_t dt = now_ms - s_pid.last_update_ms;
    if (dt > 100) dt = 5;  // prevent huge dt after pause
    if (dt == 0)  dt = 5;  // safety: assume 200 Hz

    s_pid.last_update_ms = now_ms;

    const float output = pidCompute(&s_pid, error, dt);

    if (output < 0.0f || output > 65535.0f) {
        s_state.error_code = STEER_ERR_PID_COMPUTE;
        s_actuator_cmd = 0;
        return { false, STEER_ERR_PID_COMPUTE };
    }

    s_actuator_cmd = static_cast<uint16_t>(output);
    s_state.error_code = STEER_ERR_OK;

    // Write PID output to g_nav for network reporting.
    {
        StateLock lock;
        g_nav.pid.pid_output = s_actuator_cmd;
    }

    return MOD_OK;
}

// ===================================================================
// Pipeline: output — write actuator
// ===================================================================

static ModuleResult mod_steer_output(uint32_t now_ms) {
    (void)now_ms;

    if (!feat::act()) return MOD_OK;
    if (!moduleSysIsActive(ModuleId::ACTUATOR)) return MOD_OK;
    if (s_manual_actuator_mode) return MOD_OK;

    mod_actuator_set_cmd(s_actuator_cmd);
    return MOD_OK;
}

// ===================================================================
// Configuration: cfg_get
// ===================================================================

static bool mod_steer_cfg_get(const char* key, char* buf, size_t len) {
    if (!key || !buf || len == 0) return false;

    if (std::strcmp(key, "kp") == 0) {
        std::snprintf(buf, len, "%.4f", cfg_kp);
        return true;
    }
    if (std::strcmp(key, "ki") == 0) {
        std::snprintf(buf, len, "%.4f", cfg_ki);
        return true;
    }
    if (std::strcmp(key, "kd") == 0) {
        std::snprintf(buf, len, "%.4f", cfg_kd);
        return true;
    }
    if (std::strcmp(key, "output_min") == 0) {
        std::snprintf(buf, len, "%.1f", cfg_out_min);
        return true;
    }
    if (std::strcmp(key, "output_max") == 0) {
        std::snprintf(buf, len, "%.1f", cfg_out_max);
        return true;
    }
    return false;
}

// ===================================================================
// Configuration: cfg_set
// ===================================================================

static bool mod_steer_cfg_set(const char* key, const char* val) {
    if (!key || !val) return false;

    float fval = 0.0f;
    if (std::sscanf(val, "%f", &fval) != 1) return false;

    if (std::strcmp(key, "kp") == 0) {
        cfg_kp = fval;
        return true;
    }
    if (std::strcmp(key, "ki") == 0) {
        cfg_ki = fval;
        return true;
    }
    if (std::strcmp(key, "kd") == 0) {
        cfg_kd = fval;
        return true;
    }
    if (std::strcmp(key, "output_min") == 0) {
        if (fval < 0.0f) fval = 0.0f;
        cfg_out_min = fval;
        return true;
    }
    if (std::strcmp(key, "output_max") == 0) {
        if (fval < cfg_out_min) fval = cfg_out_min;
        cfg_out_max = fval;
        return true;
    }
    return false;
}

// ===================================================================
// Configuration: cfg_apply — push config into PidState + reset
// ===================================================================

static bool mod_steer_cfg_apply(void) {
    s_pid.kp = cfg_kp;
    s_pid.ki = cfg_ki;
    s_pid.kd = cfg_kd;
    s_pid.output_min = cfg_out_min;
    s_pid.output_max = cfg_out_max;
    pidReset(&s_pid);
    hal_log("STEER: config applied (Kp=%.4f Ki=%.4f Kd=%.4f min=%.1f max=%.1f)",
            cfg_kp, cfg_ki, cfg_kd, cfg_out_min, cfg_out_max);
    return true;
}

// ===================================================================
// Configuration: cfg_save / cfg_load
// ===================================================================

static bool mod_steer_cfg_save(void) {
    // TODO: persist to NVS
    hal_log("STEER: cfg_save (NVS not yet implemented)");
    return true;
}

static bool mod_steer_cfg_load(void) {
    // TODO: load from NVS; for now use compile-time defaults
    hal_log("STEER: cfg_load (using defaults)");
    return true;
}

// ===================================================================
// Configuration: cfg_show
// ===================================================================

static bool mod_steer_cfg_show(void) {
    hal_log("STEER config:");
    hal_log("  kp         = %.4f", cfg_kp);
    hal_log("  ki         = %.4f", cfg_ki);
    hal_log("  kd         = %.4f", cfg_kd);
    hal_log("  output_min = %.1f", cfg_out_min);
    hal_log("  output_max = %.1f", cfg_out_max);
    return true;
}

// ===================================================================
// Debug
// ===================================================================

static bool mod_steer_debug(void) {
    hal_log("STEER debug:");
    hal_log("  detected      = %s", s_state.detected ? "yes" : "no");
    hal_log("  quality_ok    = %s", s_state.quality_ok ? "yes" : "no");
    hal_log("  error_code    = %lu", (unsigned long)s_state.error_code);
    hal_log("  last_update   = %lu ms", (unsigned long)s_state.last_update_ms);
    hal_log("  actuator_cmd  = %u", (unsigned)s_actuator_cmd);
    hal_log("  heading       = %.2f deg", s_snap.heading_deg);
    hal_log("  yaw_rate      = %.2f dps", s_snap.yaw_rate_dps);
    hal_log("  roll          = %.2f deg", s_snap.roll_deg);
    hal_log("  steer_angle   = %.2f deg", s_snap.steer_angle_deg);
    hal_log("  steer_raw     = %d", (int)s_snap.steer_angle_raw);
    hal_log("  desired_angle = %.2f deg", s_snap.desired_angle_deg);
    hal_log("  safety        = %s", s_snap.safety_ok ? "OK" : "KICK");
    hal_log("  watchdog      = %s", s_snap.watchdog_triggered ? "TRIG" : "OK");
    hal_log("  speed         = %.1f km/h", s_snap.speed_kmh);
    hal_log("  work/steer    = %s/%s",
            s_snap.work_switch ? "ON" : "off",
            s_snap.steer_switch ? "ON" : "off");
    hal_log("  pid_integral   = %.6f", s_pid.integral);
    hal_log("  pid_prev_error = %.4f", s_pid.prev_error);
    hal_log("  manual_mode    = %s", s_manual_actuator_mode ? "yes" : "no");
    return true;
}

// ===================================================================
// Public helpers
// ===================================================================

void mod_steer_reset_pid(void) {
    pidReset(&s_pid);
    s_actuator_cmd = 0;
}

uint16_t mod_steer_get_cmd(void) {
    return s_actuator_cmd;
}

void mod_steer_set_pid_gains(float kp, float ki, float kd) {
    cfg_kp = kp;
    cfg_ki = ki;
    cfg_kd = kd;
    s_pid.kp = kp;
    s_pid.ki = ki;
    s_pid.kd = kd;
    pidReset(&s_pid);
    hal_log("STEER: PID gains set via CLI: Kp=%.3f Ki=%.3f Kd=%.3f", kp, ki, kd);
}

void mod_steer_set_pid_output_limits(float out_min, float out_max) {
    if (out_min < 0.0f) out_min = 0.0f;
    if (out_max < out_min) out_max = out_min;
    cfg_out_min = out_min;
    cfg_out_max = out_max;
    s_pid.output_min = out_min;
    s_pid.output_max = out_max;
    pidReset(&s_pid);
    hal_log("STEER: PID output limits set via CLI: min=%.1f max=%.1f", out_min, out_max);
}

void mod_steer_get_pid_gains(float* kp, float* ki, float* kd) {
    if (kp) *kp = cfg_kp;
    if (ki) *ki = cfg_ki;
    if (kd) *kd = cfg_kd;
}

void mod_steer_apply_agio_settings(uint8_t kp, uint8_t highPWM, uint8_t lowPWM,
                                    uint8_t minPWM, uint8_t countsPerDegree,
                                    int16_t wasOffset, uint8_t ackerman) {
    // AgOpenGPS sends Kp as raw value (e.g. 30 = Kp 3.0)
    float new_kp = static_cast<float>(kp);

    float new_out_min = static_cast<float>(minPWM);
    float new_out_max = static_cast<float>(highPWM);

    // Only update if values actually changed
    if (new_kp != s_pid.kp ||
        new_out_min != s_pid.output_min ||
        new_out_max != s_pid.output_max) {

        cfg_kp = new_kp;
        cfg_out_min = new_out_min;
        cfg_out_max = new_out_max;
        s_pid.kp = new_kp;
        s_pid.output_min = new_out_min;
        s_pid.output_max = new_out_max;

        // Reset integral on gain change to prevent windup from old gains
        s_pid.integral = 0.0f;
        s_pid.prev_error = 0.0f;
        s_pid.first_update = true;

        uint8_t effective_lowPWM = static_cast<uint8_t>(minPWM * 1.2f);
        hal_log("STEER: settings updated Kp=%.0f hiPWM=%u loPWM=%u(eff=%u) minPWM=%u counts=%u ack=%u",
                (float)kp, (unsigned)highPWM, (unsigned)lowPWM,
                (unsigned)effective_lowPWM, (unsigned)minPWM,
                (unsigned)countsPerDegree, (unsigned)ackerman);
    }

    // Store all settings in global state for status reporting
    {
        StateLock lock;
        g_nav.pid.settings_kp           = kp;
        g_nav.pid.settings_high_pwm     = highPWM;
        g_nav.pid.settings_low_pwm      = lowPWM;
        g_nav.pid.settings_min_pwm      = minPWM;
        g_nav.pid.settings_counts       = countsPerDegree;
        g_nav.pid.settings_was_offset   = wasOffset;
        g_nav.pid.settings_ackerman     = ackerman;
        g_nav.pid.settings_received     = true;
    }
}

void mod_steer_set_manual_actuator_mode(bool enabled) {
    s_manual_actuator_mode = enabled;
    if (enabled) {
        pidReset(&s_pid);
        s_actuator_cmd = 0;
    }
}

bool mod_steer_manual_actuator_mode(void) {
    return s_manual_actuator_mode;
}

// ===================================================================
// Config key definitions
// ===================================================================
static const CfgKeyDef s_steer_keys[] = {
    {"kp",         "Proportional gain"},
    {"ki",         "Integral gain"},
    {"kd",         "Derivative gain"},
    {"output_min", "Minimum PID output"},
    {"output_max", "Maximum PID output"},
    {nullptr, nullptr}  // sentinel
};

static const CfgKeyDef* mod_steer_cfg_keys(void) { return s_steer_keys; }

// ===================================================================
// Ops table — const, ModuleOps2 (15 function pointers)
// ===================================================================

const ModuleOps2 mod_steer_ops = {
    .name        = "STEER",
    .id          = ModuleId::STEER,

    .is_enabled  = mod_steer_is_enabled,
    .activate    = mod_steer_activate,
    .deactivate  = mod_steer_deactivate,
    .is_healthy  = mod_steer_is_healthy,

    .input       = mod_steer_input,
    .process     = mod_steer_process,
    .output      = mod_steer_output,

    .cfg_keys    = mod_steer_cfg_keys,
    .cfg_get     = mod_steer_cfg_get,
    .cfg_set     = mod_steer_cfg_set,
    .cfg_apply   = mod_steer_cfg_apply,
    .cfg_save    = mod_steer_cfg_save,
    .cfg_load    = mod_steer_cfg_load,
    .cfg_show    = mod_steer_cfg_show,

    .debug       = mod_steer_debug,

    .deps        = s_deps,
};
