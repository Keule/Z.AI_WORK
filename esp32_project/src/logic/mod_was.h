/**
 * @file mod_was.h
 * @brief Wheel Angle Sensor module — ADS1118 over SPI (ModuleOps2 interface).
 *
 * Migrated from was.h/was.cpp to the unified module system.
 * Error codes: 1=not detected, 2=read failed, 3=implausible data
 */

#pragma once

#include "module_interface.h"

/// Module ops table for the WAS module.
extern const ModuleOps2 mod_was_ops;
