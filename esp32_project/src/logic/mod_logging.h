/**
 * @file mod_logging.h
 * @brief Module LOGGING — SD-Card data logger (ModuleOps2).
 *
 * Wraps the existing sd_logger ring-buffer / maintenance-task system
 * as a ModuleOps2 module.  The heavy lifting (ring buffer, SD writes)
 * stays in sd_logger.cpp; this module only provides the lifecycle and
 * pipeline glue.
 *
 * Error codes:
 *   1 = sd_not_present
 *   2 = write_error
 *   3 = buffer_overflow
 */

#pragma once

#include "module_interface.h"

/// Ops table — declared in module_system.cpp via extern.
extern const ModuleOps2 mod_logging_ops;

/// Check if the SD card was detected at boot.
bool mod_logging_sd_detected(void);

/// Get the number of records flushed to SD (delegates to sdLoggerGetRecordsFlushed).
uint32_t mod_logging_records_flushed(void);
