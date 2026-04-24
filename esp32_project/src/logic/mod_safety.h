/**
 * @file mod_safety.h
 * @brief Safety loop monitoring module (ModuleOps2 interface).
 *
 * Monitors the safety circuit GPIO and watchdog timer.
 * Error codes: 1=safety_kick, 2=watchdog_triggered
 */

#pragma once

#include "module_interface.h"

/// Module ops table for the SAFETY module.
extern const ModuleOps2 mod_safety_ops;
