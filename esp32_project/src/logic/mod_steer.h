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
