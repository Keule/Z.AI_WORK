/**
 * @file op_mode.h
 * @brief Operating Mode State Machine — ADR-005.
 *
 * Three operating modes: BOOTING → ACTIVE ↔ PAUSED.
 * - ACTIVE: controlTask + commTask running at full rate
 * - PAUSED: controlTask + commTask suspended, enhanced maintenance/diagnostics
 * - BOOTING: Initial hardware init and configuration
 *
 * Thread safety: Mode reads use atomic (lock-free). Mode transitions use
 * a dedicated mutex to prevent concurrent transitions.
 */

#pragma once

#include <cstdint>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Operating modes of the firmware (ADR-005).
typedef enum {
    OP_MODE_BOOTING = 0,   ///< Initial boot and hardware init
    OP_MODE_ACTIVE  = 1,   ///< Normal operation — control + comm running
    OP_MODE_PAUSED  = 2    ///< Configuration/diagnostics — control + comm suspended
} OpMode;

/// Get current operating mode (atomic read, lock-free).
OpMode opModeGet(void);

/// Request mode transition. Returns true if accepted.
/// ACTIVE → PAUSED: Only when safety == LOW AND vehicle_speed < threshold.
/// PAUSED → ACTIVE: Always allowed (after sanity check).
/// BOOTING → ACTIVE/PAUSED: Only via opModeBootComplete().
bool opModeRequest(OpMode target);

/// Called by boot sequence after init is complete.
/// If safety_low is true → PAUSED, else → ACTIVE.
void opModeBootComplete(bool safety_low);

/// Called by safety monitoring when safety state changes.
/// If safety goes LOW in ACTIVE mode AND speed < threshold → auto-transition to PAUSED.
void opModeSafetyChanged(bool safety_ok, float vehicle_speed_kmh);

/// Check if controlTask should run its full pipeline.
bool opModeIsControlActive(void);

/// Check if commTask should run its full pipeline.
bool opModeIsCommActive(void);

/// Check if we're in paused/configuration mode.
bool opModeIsPaused(void);

/// Get mode as human-readable string (German).
const char* opModeToString(OpMode mode);

/// Initialize the operating mode subsystem.
void opModeInit(void);

/// Get the NVS preference key for mode persistence.
/// Returns the preferred mode from NVS, or OP_MODE_ACTIVE as default.
OpMode opModeLoadPreference(void);

/// Save current mode preference to NVS.
void opModeSavePreference(void);

/// GPIO-Poll fuer Modus-Switch (100 ms Interval, aus maintTask aufzurufen).
void opModeGpioPoll(void);

/// GPIO Mode-Switch aktivieren/deaktivieren.
void opModeSetGpioEnabled(bool enabled);

/// Check if PAUSED-Bit im switchStatus gesetzt werden soll (fuer PGN 253).
bool opModeIsPausedStatusBit(void);

#ifdef __cplusplus
}
#endif
