/**
 * @file mod_spi.h
 * @brief SPI Shared Bus module — consumer registration API.
 *
 * The SPI_SHARED module manages the sensor SPI bus (SPI2_HOST / SENS_SPI_BUS).
 * It auto-switches between two modes based on active consumer count:
 *
 *   DIRECT mode (1 consumer):
 *     - No mutex, no multi-client CS arbitration
 *     - SPI bus stays configured with the single consumer's settings
 *     - Minimal overhead per transaction
 *
 *   SHARED mode (2+ consumers):
 *     - FreeRTOS mutex for bus arbitration
 *     - All CS pins deasserted before each transaction
 *     - beginTransaction/endTransaction per device (different modes/frequencies)
 *     - Full telemetry and client-switch tracking
 *
 * SPI consumers (IMU, WAS, ACTUATOR) must call:
 *   spi_shared_add_consumer(ModuleId::XXX)    in their activate()
 *   spi_shared_remove_consumer(ModuleId::XXX) in their deactivate()
 *
 * The module is auto-managed — it cannot be manually activated/deactivated
 * via CLI. It appears in `module list` and `module show SPI_SHARED`.
 */

#pragma once

#include "module_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===================================================================
// Consumer Registration — called by SPI-consuming modules
// ===================================================================

/// Register a module as SPI bus consumer.
/// - 1st consumer: initializes bus in DIRECT mode
/// - 2nd+ consumer: transitions to SHARED mode
/// Safe to call if already registered (idempotent).
void spi_shared_add_consumer(ModuleId id);

/// Unregister a module as SPI bus consumer.
/// - If going from 2→1: transitions back to DIRECT mode
/// - If going from 1→0: deinitializes SPI bus completely
/// Safe to call if not registered (no-op).
void spi_shared_remove_consumer(ModuleId id);

// ===================================================================
// State Queries
// ===================================================================

/// Number of currently registered SPI consumers (0-3).
uint8_t spi_shared_consumer_count(void);

/// Whether the SPI bus is currently initialized.
bool spi_shared_is_initialized(void);

/// Whether the bus is in DIRECT (single-consumer) mode.
/// Returns false if SHARED mode or bus not initialized.
bool spi_shared_is_direct_mode(void);

/// Whether the bus is in SHARED (multi-consumer) mode.
bool spi_shared_is_shared_mode(void);

/// Get the name of the sole consumer in DIRECT mode, or nullptr.
const char* spi_shared_direct_consumer_name(void);

#ifdef __cplusplus
}

// Module ops table (for module_system.cpp registration)
extern const ModuleOps2 mod_spi_shared_ops;

#endif
