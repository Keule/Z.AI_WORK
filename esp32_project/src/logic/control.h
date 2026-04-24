/**
 * @file control.h
 * @brief PID controller and 200 Hz control loop.
 *
 * Reads sensors, computes PID, writes actuator.
 * Safety monitoring is integrated.
 */

#pragma once

#include <cstdint>

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

/// Initialise PID with default parameters.
void pidInit(PidState* pid,
             float kp = 1.0f, float ki = 0.0f, float kd = 0.0f,
             float out_min = 0.0f, float out_max = 65535.0f);

/// Reset PID internal state (integral, prev_error).
void pidReset(PidState* pid);

/// Compute PID output.
/// @param pid     PID state
/// @param error   current error (setpoint - measurement)
/// @param dt_ms   time step in milliseconds
/// @return computed output value
float pidCompute(PidState* pid, float error, uint32_t dt_ms);

/// Initialise control subsystem (IMU, steer angle sensor, actuator).
void controlInit(void);

/// Run one control step. Should be called at 200 Hz.
/// Reads sensors, checks safety, computes PID, writes actuator.
void controlStep(void);

// ===================================================================
// Phase Functions — Phase 4 (extracted from controlStep)
// ===================================================================

/// Sensor data snapshot — filled by controlReadSensors().
struct SensorSnapshot {
    float    was_angle_deg    = 0.0f;
    int16_t  was_raw          = 0;
    uint32_t was_timestamp_ms = 0;
    bool     was_quality      = false;
    uint32_t imu_timestamp_ms = 0;
    bool     imu_quality      = false;
};

/// AgIO input snapshot — read from g_nav.sw.
struct AgioInputSnapshot {
    bool     auto_steer_enabled = false;
    float    gps_speed_kmh      = 0.0f;
    uint32_t watchdog_timer_ms  = 0;
    float    setpoint_deg       = 0.0f;
};

/// PID computation result.
struct PidResult {
    uint16_t actuator_cmd = 0;
    bool     reset_pid    = false;
};

/// Phase 1: Read safety circuit (always).
bool controlReadSafety(void);

/// Phase 2: Read sensors via module interface (feature-gated).
void controlReadSensors(SensorSnapshot& snap);

/// Phase 3: Check watchdog timeout (always).
bool controlCheckWatchdog(uint32_t now_ms, uint32_t watchdog_timer_ms);

/// Phase 4: Compute PID output (feature-gated).
void controlComputePid(const SensorSnapshot& snap,
                       const AgioInputSnapshot& agio,
                       bool safety_ok,
                       bool watchdog_triggered,
                       uint32_t now_ms,
                       PidResult& result);

/// Phase 5: Write actuator command via SPI (feature-gated).
void controlWriteActuator(uint16_t actuator_cmd);

/// Phase 6: Write all control outputs to g_nav (always).
void controlWriteState(uint32_t now_ms,
                       bool safety_ok,
                       bool watchdog_triggered,
                       const SensorSnapshot& snap,
                       const PidResult& result);

/// Update PID gains and actuator limits from AgIO steer settings (PGN 252).
/// AgOpenGPS v5 sends: Kp(uint8), HighPWM(uint8), LowPWM(uint8),
/// MinPWM(uint8), CountsPerDegree(uint8), WASOffset(int16), Ackerman(uint8).
/// @param kp              proportional gain (raw)
/// @param highPWM         maximum actuator PWM
/// @param lowPWM          deadband / no-action PWM band
/// @param minPWM          minimum actuator PWM for instant on
/// @param countsPerDegree sensor counts per degree
/// @param wasOffset       sensor zero offset (counts)
/// @param ackerman        Ackerman correction factor (value / 100.0)
void controlUpdateSettings(uint8_t kp, uint8_t highPWM, uint8_t lowPWM,
                           uint8_t minPWM, uint8_t countsPerDegree,
                           int16_t wasOffset, uint8_t ackerman);

/// Update PID gains at runtime (Serial CLI tuning path).
void controlSetPidGains(float kp, float ki, float kd);

/// Update PID output clamps at runtime.
void controlSetPidOutputLimits(float out_min, float out_max);

/// Read current PID tuning values.
void controlGetPidGains(float* kp, float* ki, float* kd);

/// Enable/disable manual actuator test mode (disables control write path).
void controlSetManualActuatorMode(bool enabled);

/// Query manual actuator mode.
bool controlManualActuatorMode(void);

// ===================================================================
// Globals
// ===================================================================

// ADR-STATE-001: desiredSteerAngleDeg wurde in g_nav.sw verschoben.
// Lesezugriff ueber StateLock in controlStep().
