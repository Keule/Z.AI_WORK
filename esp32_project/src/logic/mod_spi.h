/**
 * @file mod_spi.h
 * @brief SPI bus module — consumer registration and dual-module API.
 *
 * Two infrastructure modules manage the sensor SPI bus:
 *
 *   SPI (single-consumer / DIRECT mode):
 *     - Bus initialized with first consumer's settings
 *     - No mutex, no multi-client CS arbitration
 *     - Minimal overhead per transaction
 *     - Active whenever ≥1 SPI device needs the bus
 *
 *   SPI_SHARED (multi-client / SHARED mode):
 *     - Depends on SPI module (requires bus to be initialized)
 *     - Enables FreeRTOS mutex + all-CS deassert per transaction
 *     - beginTransaction/endTransaction per device (different modes/frequencies)
 *     - Full telemetry and client-switch tracking
 *     - Active only when ≥2 SPI devices need the bus simultaneously
 *
 * SPI consumers (IMU, WAS, ACTUATOR) call:
 *   spi_add_consumer(ModuleId::XXX)    in their activate()
 *   spi_remove_consumer(ModuleId::XXX) in their deactivate()
 *
 * The consumer API auto-manages both modules:
 *   0→1 consumers: activate SPI
 *   1→2 consumers: activate SPI_SHARED
 *   2→1 consumers: deactivate SPI_SHARED
 *   1→0 consumers: deactivate SPI
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
/// 1st consumer → activates SPI module (DIRECT mode)
/// 2nd+ consumer → activates SPI_SHARED module (SHARED mode)
/// Idempotent: safe to call if already registered.
void spi_add_consumer(ModuleId id);

/// Unregister a module as SPI bus consumer.
/// 2→1 consumers → deactivates SPI_SHARED (back to DIRECT)
/// 1→0 consumers → deactivates SPI (bus deinit)
/// Idempotent: safe to call if not registered.
void spi_remove_consumer(ModuleId id);

// ===================================================================
// State Queries
// ===================================================================

/// Number of currently registered SPI consumers (0-3).
uint8_t spi_consumer_count(void);

/// Whether the SPI bus is currently initialized.
bool spi_is_initialized(void);

/// Whether the bus is in DIRECT (single-consumer) mode.
bool spi_is_direct_mode(void);

/// Whether the bus is in SHARED (multi-client) mode.
bool spi_is_shared_mode(void);

#ifdef __cplusplus
}

// Module ops tables (for module_system.cpp registration)
extern const ModuleOps2 mod_spi_ops;
extern const ModuleOps2 mod_spi_shared_ops;

#endif
