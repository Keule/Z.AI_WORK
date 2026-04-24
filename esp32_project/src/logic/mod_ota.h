/**
 * @file mod_ota.h
 * @brief Module OTA — Over-The-Air firmware update (ModuleOps2).
 *
 * Minimal stub module.  The actual OTA logic (HTTP server, SD-card update)
 * stays in main.cpp for now and will be migrated into this module later.
 *
 * This module only serves as a registration point in the module system.
 * It depends on ETH (at least one transport must be available).
 *
 * Error codes:
 *   0 = no errors (placeholder)
 */

#pragma once

#include "module_interface.h"

/// Ops table — declared in module_system.cpp via extern.
extern const ModuleOps2 mod_ota_ops;
