/**
 * @file actuator.h
 * @brief Steering actuator driver over SPI.
 *
 * Writes a command (0..65535) to the steering actuator.
 */

#pragma once

#include <cstdint>

#include "features.h"
#include "module_interface.h"

/// Compile-time check: is actuator compiled in?
constexpr bool actuatorIsEnabled() { return feat::act(); }

/// Initialise actuator SPI interface.
void actuatorInit(void);

/// Write command to actuator via SPI. Returns true on success.
bool actuatorUpdate(uint16_t cmd);

/// Actuator has no freshness concept — always healthy if enabled.
bool actuatorIsHealthy(uint32_t now_ms);

/// Module registry entry for actuator.
extern const ModuleOps actuator_ops;

// --- Legacy compatibility (deprecated) ---
inline void actuatorWriteCommand(uint16_t cmd) { (void)actuatorUpdate(cmd); }
