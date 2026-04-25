/**
 * @file mod_remote_console.h
 * @brief Infrastructure module: TCP/Telnet Remote Console (ModuleId::REMOTE_CONSOLE).
 *
 * Wraps DebugConsole (DBG) as a proper module with compile-time feature guard
 * (FEAT_REMOTE_CONSOLE) and runtime enable/disable via module system.
 *
 * CLI: "remoteconsole <on|off|show>"
 * Module: "module REMOTE_CONSOLE show|activate|deactivate"
 */

#pragma once

#include "module_interface.h"
#include "features.h"

#if FEAT_ENABLED(FEAT_COMPILED_REMOTE_CONSOLE)

/// Module ops table for remote console.
extern const ModuleOps2 mod_remote_console_ops;

#endif  // FEAT_ENABLED(FEAT_COMPILED_REMOTE_CONSOLE)
