/**
 * @file mod_steer.h
 * @brief Module STEER — PID steering controller (ModuleOps2).
 *
 * Migrated from control.h / control.cpp. Reads sensor snapshots from g_nav
 * (populated by IMU/WAS/SAFETY input phases), computes PID, writes actuator.
 *
 * Error codes:
 *   1 = pid_compute_error
 *   2 = safety_kick
 *   3 = watchdog_triggered
 */

#pragma once

#include "module_interface.h"

/// Ops table — declared in module_system.cpp via extern.
extern const ModuleOps2 mod_steer_ops;

/// Reset PID internal state (public for diagnostics / CLI).
void mod_steer_reset_pid(void);

/// Get current PID command output (for diagnostics).
uint16_t mod_steer_get_cmd(void);

/// Set PID gains at runtime (for config_pid.cpp CLI path).
/// Resets integral on gain change.
void mod_steer_set_pid_gains(float kp, float ki, float kd);

/// Set PID output clamps at runtime.
void mod_steer_set_pid_output_limits(float out_min, float out_max);

/// Read current PID tuning values.
void mod_steer_get_pid_gains(float* kp, float* ki, float* kd);

/// Apply AgIO steer settings (PGN 252) — updates PID gains + output limits.
/// Stores raw settings in g_nav.pid for status reporting.
void mod_steer_apply_agio_settings(uint8_t kp, uint8_t highPWM, uint8_t lowPWM,
                                    uint8_t minPWM, uint8_t countsPerDegree,
                                    int16_t wasOffset, uint8_t ackerman);

/// Enable/disable manual actuator test mode (disables PID output).
void mod_steer_set_manual_actuator_mode(bool enabled);

/// Query manual actuator mode.
bool mod_steer_manual_actuator_mode(void);
