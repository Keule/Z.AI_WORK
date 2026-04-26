/**
 * @file sd_logger.h
 * @brief SD-Card Data Logger – public API.
 *
 * Records navigation/steering data to CSV files on an SD card.
 * Activated/deactivated via a hardware switch (GPIO 47, active LOW).
 *
 * Architecture (TASK-029):
 *   - Control loop calls sdLoggerRecord() to buffer one sample into
 *     a PSRAM-backed ring buffer (~1 MB, ~53 min at 10 Hz)
 *   - task_slow (Core 0) periodically calls sdLoggerTick() every 2 s
 *     to drain the ring buffer and write CSV records to the SD card
 *   - task_slow also handles NTRIP connect/reconnect and ETH monitoring
 *   - The SD card shares SPI2_HOST (SD_SPI_BUS) with the sensor bus,
 *     so the sensor SPI is temporarily released during writes
 *   - The hardware switch is checked before each flush cycle
 *
 * Usage:
 *   sdLoggerMaintInit(); // call once in setup() after OTA check
 *   // In control loop:
 *   sdLoggerRecord();    // buffer one sample (subsampled internally)
 *   // In task_slow (every 2 s):
 *   sdLoggerTick();      // flush ring buffer to SD card
 */

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ===================================================================
// Log record – one sample of system state
// ===================================================================

/// A single log record – compact binary representation.
/// Written to ring buffer by the control loop, converted to CSV
/// by the logger when flushing to SD card.
typedef struct {
    uint32_t timestamp_ms;   ///< milliseconds since boot
    float   heading_deg;     ///< heading [degrees, 0-360]
    float   steer_angle_deg; ///< measured steering angle [degrees]
    float   desired_angle_deg; ///< desired steering angle (setpoint) [degrees]
    float   yaw_rate_dps;    ///< yaw rate from IMU [deg/s]
    float   roll_deg;        ///< roll angle [degrees]
    uint8_t safety_ok;       ///< 1 = safety circuit OK, 0 = KICK
} SdLogRecord;

// ===================================================================
// Public API
// ===================================================================

/// Initialise the SD logger (legacy — no task creation).
///
/// Sets up GPIO 47 as input with pull-up (logging switch).
/// Uses the internal static 16 KB ring buffer.
///
/// Must be called AFTER hal_esp32_init_all() but BEFORE
/// creating the control/comm tasks.
///
/// Prefer sdLoggerMaintInit() for TASK-029 builds (PSRAM buffer
/// + task_slow integration).
void sdLoggerInit(void);

/// Buffer one log record.
///
/// Called from the control loop (200 Hz). Internally subsamples
/// to the configured log rate (default: 10 Hz = every 20th call).
/// The function is very fast (~1 µs) – just a memcpy into the
/// ring buffer.
///
/// If the ring buffer is full, the oldest record is silently
/// overwritten (ring buffer wrap-around).
///
/// Safe to call even if logging is disabled – it will be a no-op
/// after checking the switch state (first call in a subsample group).
void sdLoggerRecord(void);

/// Check if logging is currently active (switch is ON).
///
/// Returns true if GPIO 47 is LOW (switch closed to GND).
bool sdLoggerIsActive(void);

/// Get the number of records written to SD card since boot.
/// Useful for status reporting.
uint32_t sdLoggerGetRecordsFlushed(void);

/// Get the number of records currently in the ring buffer
/// (not yet flushed to SD).
uint32_t sdLoggerGetBufferCount(void);

/// Redirect the ring buffer to an externally-allocated buffer (e.g. PSRAM).
///
/// After calling this, all ring buffer operations use the provided buffer
/// instead of the static fallback. The existing indices are clamped to the
/// new capacity boundary.
///
/// @param buf       Pointer to the ring buffer (must be non-null).
/// @param capacity  Buffer capacity (must be power of 2, non-zero).
void sdLoggerSetExternalBuffer(SdLogRecord* buf, uint32_t capacity);

// ===================================================================
// TASK-029: PSRAM Ring Buffer + task_slow Integration
// ===================================================================

/// Allocate the PSRAM-backed ring buffer.
///
/// Idempotent — safe to call multiple times (Issue 3: double-init guard).
/// Returns true if a large buffer (PSRAM or heap) was successfully allocated.
///
/// Called by sdLoggerMaintInit() and directly by mod_logging.activate().
bool sdLoggerInitPsram(void);

/// One iteration of the SD flush state machine.
/// Called from task_slow every 2 s.
///
/// Handles switch debounce, SD card SPI bus claim/release,
/// log file open/close, and ring buffer → CSV flush.
void sdLoggerTick(void);

/// ETH link monitoring — log on link UP/DOWN transitions.
/// Called from task_slow every ~1 s.
void sdLoggerEthMonitor(void);

/// Initialise the maintenance functions (backward-compat wrapper).
///
/// Calls sdLoggerInitPsram() to allocate the PSRAM ring buffer.
///
/// Must be called AFTER hal_esp32_init_all() but BEFORE
/// creating the control/comm tasks.
void sdLoggerMaintInit(void);

/// Check if PSRAM ring buffer is active (successfully allocated).
/// Returns true when the ring buffer was allocated from PSRAM or
/// regular heap (i.e. larger than the static fallback).
bool sdLoggerPsramBufferActive(void);

/// Get current PSRAM buffer fill count (diagnostics).
/// Returns 0 if PSRAM buffer is not active.
uint32_t sdLoggerPsramBufferCount(void);

#ifdef __cplusplus
}
#endif
