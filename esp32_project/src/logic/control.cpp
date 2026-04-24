/**
 * @file control.cpp
 * @brief PID controller and control loop implementation.
 *
 * Runs at 200 Hz on Core 1.
 * Sequence: Safety -> IMU -> Steer Angle -> Watchdog -> PID -> Actuator
 */

#include "control.h"
#include "dependency_policy.h"
#include "imu.h"
#include "was.h"
#include "actuator.h"
#include "global_state.h"
#include "module_interface.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CTL
#include "esp_log.h"
#include "log_ext.h"

#include <cmath>

// ===================================================================
// Constants
// ===================================================================

/// Minimum GPS speed [km/h] to allow auto-steering.
/// Reference disables steering below 0.1 km/h for safety.
constexpr float MIN_STEER_SPEED_KMH = 0.1f;

// ===================================================================
// Globals – actual definition in global_state.cpp
// ===================================================================

/// PID instance for steering
static PidState s_steer_pid;
static uint32_t s_last_was_diag_ms = 0;
static bool s_manual_actuator_mode = false;
static bool s_last_safety_logged = true;
static bool s_safety_log_init = false;

// NOTE: controlStep() is retained for backward compatibility but the
// new module pipeline (mod_safety, mod_imu, mod_was, mod_steer, mod_actuator)
// supersedes it. Only controlInit() and PID math functions are actively used.

// ===================================================================
// PID Implementation
// ===================================================================

void pidInit(PidState* pid, float kp, float ki, float kd,
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

void pidReset(PidState* pid) {
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->first_update = true;
}

float pidCompute(PidState* pid, float error, uint32_t dt_ms) {
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
// Control System
// ===================================================================

void controlInit(void) {
    // NOTE: HAL-level init (hal_imu_begin, hal_steer_angle_begin,
    // hal_actuator_begin) is already done in hal_esp32_init_all().
    // We only need to init the PID controller here.

    // Default PID gains – tune for actual actuator/sensor combo
    pidInit(&s_steer_pid,
            1.0f,    // Kp
            0.0f,    // Ki
            0.01f,   // Kd
            0.0f,    // min output
            65535.0f // max output
    );

    hal_log("Control: initialised (PID Kp=%.2f Ki=%.3f Kd=%.3f)",
            s_steer_pid.kp, s_steer_pid.ki, s_steer_pid.kd);
}

void controlUpdateSettings(uint8_t kp, uint8_t highPWM, uint8_t lowPWM,
                           uint8_t minPWM, uint8_t countsPerDegree,
                           int16_t wasOffset, uint8_t ackerman) {
    // AgOpenGPS sends Kp as raw value (e.g. 30 = Kp 3.0)
    float new_kp = kp;

    // Reference applies: lowPWM = minPWM * 1.2 (overrides AgIO value)
    uint8_t effective_lowPWM = static_cast<uint8_t>(minPWM * 1.2f);

    float new_out_min = static_cast<float>(minPWM);
    float new_out_max = static_cast<float>(highPWM);

    // Only update if values actually changed
    if (new_kp != s_steer_pid.kp ||
        new_out_min != s_steer_pid.output_min ||
        new_out_max != s_steer_pid.output_max) {

        s_steer_pid.kp = new_kp;
        s_steer_pid.output_min = new_out_min;
        s_steer_pid.output_max = new_out_max;

        // Reset integral on gain change to prevent windup from old gains
        s_steer_pid.integral = 0.0f;
        s_steer_pid.prev_error = 0.0f;
        s_steer_pid.first_update = true;

        LOGI("CTL", "settings updated Kp=%.0f hiPWM=%u loPWM=%u(eff=%u) minPWM=%u counts=%u ack=%u",
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

void controlSetPidGains(float kp, float ki, float kd) {
    s_steer_pid.kp = kp;
    s_steer_pid.ki = ki;
    s_steer_pid.kd = kd;
    pidReset(&s_steer_pid);
    LOGI("CTL", "PID gains set via CLI: Kp=%.3f Ki=%.3f Kd=%.3f", kp, ki, kd);
}

void controlSetPidOutputLimits(float out_min, float out_max) {
    if (out_min < 0.0f) out_min = 0.0f;
    if (out_max < out_min) out_max = out_min;
    s_steer_pid.output_min = out_min;
    s_steer_pid.output_max = out_max;
    pidReset(&s_steer_pid);
    LOGI("CTL", "PID output limits set via CLI: min=%.1f max=%.1f", out_min, out_max);
}

void controlGetPidGains(float* kp, float* ki, float* kd) {
    if (kp) *kp = s_steer_pid.kp;
    if (ki) *ki = s_steer_pid.ki;
    if (kd) *kd = s_steer_pid.kd;
}

void controlSetManualActuatorMode(bool enabled) {
    s_manual_actuator_mode = enabled;
    if (enabled) {
        pidReset(&s_steer_pid);
    }
}

bool controlManualActuatorMode(void) {
    return s_manual_actuator_mode;
}

bool controlReadSafety(void) {
    const bool safety_active = moduleSysIsActive(ModuleId::SAFETY);
    const bool safety_ok = safety_active ? hal_safety_ok() : true;

    if (!s_safety_log_init) {
        s_last_safety_logged = safety_ok;
        s_safety_log_init = true;
    } else if (safety_ok != s_last_safety_logged) {
        LOGW("CTL", "SAFETY: %s -> %s",
             s_last_safety_logged ? "OK" : "KICK",
             safety_ok ? "OK" : "KICK");

        // NOTE: OpMode safety auto-transition is now handled by the module system.
        // When safety goes LOW in WORK mode, the mode will be managed externally.

        s_last_safety_logged = safety_ok;
    }

    {
        StateLock lock;
        g_nav.safety.safety_ok = safety_ok;
    }

    return safety_ok;
}

void controlReadSensors(SensorSnapshot& snap) {
    // Read IMU via direct function call (mod_imu.input() handles this in pipeline)
    if (moduleSysIsActive(ModuleId::IMU)) {
        (void)imuUpdate();
    }
    // Read WAS via direct function call (mod_was.input() handles this in pipeline)
    if (moduleSysIsActive(ModuleId::WAS)) {
        (void)wasUpdate();
    }

    {
        StateLock lock;
        snap.imu_timestamp_ms = g_nav.imu.imu_timestamp_ms;
        snap.imu_quality = g_nav.imu.imu_quality_ok;
    }

    const bool ads_active = feat::ads() && moduleSysIsActive(ModuleId::WAS);
    if (ads_active) {
        snap.was_angle_deg = wasGetAngleDeg();
        snap.was_raw = wasGetRaw();
        snap.was_timestamp_ms = wasGetTimestampMs();
        snap.was_quality = wasGetQuality();
    }
}

bool controlCheckWatchdog(uint32_t now_ms, uint32_t watchdog_timer_ms) {
    return (watchdog_timer_ms != 0u) &&
           (now_ms - watchdog_timer_ms > dep_policy::WATCHDOG_TIMEOUT_MS);
}

void controlComputePid(const SensorSnapshot& snap,
                       const AgioInputSnapshot& agio,
                       bool safety_ok,
                       bool watchdog_triggered,
                       uint32_t now_ms,
                       PidResult& result) {
    const bool steer_possible =
        moduleSysIsActive(ModuleId::ACTUATOR) &&
        moduleSysIsActive(ModuleId::WAS) &&
        moduleSysIsActive(ModuleId::IMU) &&
        !s_manual_actuator_mode &&
        safety_ok &&
        agio.auto_steer_enabled &&
        !watchdog_triggered &&
        agio.gps_speed_kmh >= MIN_STEER_SPEED_KMH;

    if (!steer_possible) {
        result.actuator_cmd = 0;
        result.reset_pid = true;
        return;
    }

    float error = agio.setpoint_deg - snap.was_angle_deg;
    while (error > 180.0f)  error -= 360.0f;
    while (error < -180.0f) error += 360.0f;

    uint32_t dt = now_ms - s_steer_pid.last_update_ms;
    if (dt > 100) dt = 5;  // prevent huge dt after pause
    s_steer_pid.last_update_ms = now_ms;

    const float output = pidCompute(&s_steer_pid, error, dt);
    result.actuator_cmd = static_cast<uint16_t>(output);
    result.reset_pid = false;
}

void controlWriteActuator(uint16_t actuator_cmd) {
    if (!feat::act() || !moduleSysIsActive(ModuleId::ACTUATOR)) {
        return;
    }
    if (s_manual_actuator_mode) {
        return;
    }
    (void)actuatorUpdate(actuator_cmd);
}

void controlWriteState(uint32_t now_ms,
                       bool safety_ok,
                       bool watchdog_triggered,
                       const SensorSnapshot& snap,
                       const PidResult& result) {
    if (result.reset_pid) {
        pidReset(&s_steer_pid);
    }

    controlWriteActuator(result.actuator_cmd);

    {
        const bool steer_quality_ok = feat::ads() &&
            dep_policy::isSteerAnglePlausible(snap.was_angle_deg) &&
            dep_policy::isSteerAngleRawPlausible(snap.was_raw);

        StateLock lock;
        g_nav.safety.safety_ok = safety_ok;
        g_nav.safety.watchdog_triggered = watchdog_triggered;

        g_nav.steer.steer_angle_deg = snap.was_angle_deg;
        g_nav.steer.steer_angle_raw = snap.was_raw;
        g_nav.steer.steer_angle_timestamp_ms = now_ms;
        g_nav.steer.steer_angle_quality_ok = steer_quality_ok && snap.was_quality;

        g_nav.pid.pid_output = result.actuator_cmd;
    }
}

void controlStep(void) {
    const uint32_t now_ms = hal_millis();

    // Phase 1: safety (always)
    const bool safety_ok = controlReadSafety();

    // Phase 2: sensors
    SensorSnapshot snap;
    controlReadSensors(snap);

    // AgIO snapshot — alle Felder inkl. desiredSteerAngleDeg unter StateLock
    AgioInputSnapshot agio;
    {
        StateLock lock;
        agio.auto_steer_enabled = g_nav.sw.work_switch && g_nav.sw.steer_switch;
        agio.gps_speed_kmh = g_nav.sw.gps_speed_kmh;
        agio.watchdog_timer_ms = g_nav.sw.watchdog_timer_ms;
        agio.setpoint_deg = g_nav.sw.desiredSteerAngleDeg;  // ADR-STATE-001: StateLock-geschuetzt
    }

    // Phase 3: watchdog
    const bool watchdog_triggered = controlCheckWatchdog(now_ms, agio.watchdog_timer_ms);

    // Phase 4: PID
    PidResult result;
    if (feat::act() && feat::safety()) {
        controlComputePid(snap, agio, safety_ok, watchdog_triggered, now_ms, result);
    } else {
        result = {0, true};
    }

    // Phase 5 + 6: actuator + state write
    controlWriteState(now_ms, safety_ok, watchdog_triggered, snap, result);

#if LOG_WAS_DIAG_INTERVAL_MS > 0
    if (now_ms - s_last_was_diag_ms >= LOG_WAS_DIAG_INTERVAL_MS) {
        s_last_was_diag_ms = now_ms;
        hal_log("WAS-DIAG: angle=%.2f raw=%d safety=%s",
                snap.was_angle_deg,
                (int)snap.was_raw,
                safety_ok ? "OK" : "KICK");
    }
#endif
}
