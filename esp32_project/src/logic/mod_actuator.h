/**
 * @file mod_actuator.h
 * @brief Steering actuator module — DRV8263 SPI (ModuleOps2 interface).
 *
 * Migrated from actuator.h/actuator.cpp to the unified module system.
 * Error codes: 1=not detected, 2=write failed
 */

#pragma once

#include <cstdint>
#include "module_interface.h"

/// Module ops table for the ACTUATOR module.
extern const ModuleOps2 mod_actuator_ops;

/// Set the command value to be written on the next output() cycle.
/// Typically called by mod_steer after PID computation.
void mod_actuator_set_cmd(uint16_t cmd);

/// Get the current command value.
uint16_t mod_actuator_get_cmd(void);
