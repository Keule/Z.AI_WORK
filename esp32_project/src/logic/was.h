/**
 * @file was.h
 * @brief Wheel Angle Sensor (WAS) driver — ADS1118 over SPI.
 *
 * Unified module interface: wasIsEnabled, wasInit, wasUpdate, wasIsHealthy.
 */

#pragma once

#include <cstdint>

#include "features.h"
#include "module_interface.h"

/// Compile-time check: is WAS compiled in?
constexpr bool wasIsEnabled() { return feat::ads(); }

/// Initialise WAS hardware (calls hal_steer_angle_begin).
void wasInit(void);

/// Read WAS and update internal module cache.
/// Returns true if read was successful and plausible.
bool wasUpdate(void);

/// Check if WAS data is fresh (within timeout).
bool wasIsHealthy(uint32_t now_ms);

/// Cached WAS outputs from most recent wasUpdate().
float wasGetAngleDeg(void);
int16_t wasGetRaw(void);
uint32_t wasGetTimestampMs(void);
bool wasGetQuality(void);

/// Module registry entry for WAS.
extern const ModuleOps was_ops;

// --- Legacy compatibility (deprecated, remove after Phase 3) ---
inline void steerAngleInit(void) { wasInit(); }
float steerAngleReadDeg(void);
