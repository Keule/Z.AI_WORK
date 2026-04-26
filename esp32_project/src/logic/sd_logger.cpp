/**
 * @file sd_logger.cpp
 * @brief SD-Card Data Logger – ring buffer and platform-independent logic.
 *
 * The ring buffer is a lock-free single-producer / single-consumer queue:
 *   - Producer: control loop (Core 1) calls sdLoggerRecord()
 *   - Consumer: task_slow (Core 0) calls sdLoggerHasRecords()/sdLoggerReadRecord()
 *
 * The ring buffer uses power-of-2 size so wrap-around can be done
 * with a simple mask instead of modulo.
 *
 * TASK-029: The ring buffer is pointer-based. By default it uses a
 * static fallback buffer (16 KB, 512 records). When a PSRAM buffer
 * is allocated (via sdLoggerSetExternalBuffer()), all operations
 * transparently use the larger buffer.
 *
 * This file is platform-independent – no Arduino/ESP32 headers.
 * Platform-specific code lives in sd_logger_esp32.cpp.
 */

#include "logic/sd_logger.h"
#include "logic/global_state.h"
#include <cstring>

// ===================================================================
// Configuration
// ===================================================================

/// Default (static fallback) ring buffer size (must be power of 2).
/// 512 records × 32 bytes = 16 KB.
/// At 10 Hz log rate: 51 seconds of buffer before overflow.
#define LOG_RING_SIZE_DEFAULT   512

/// Log rate divider. The control loop runs at 200 Hz.
/// A divider of 20 gives a 10 Hz log rate (one record every 100 ms).
#define LOG_RATE_DIVIDER    20

// ===================================================================
// Ring buffer – single-producer / single-consumer (pointer-based)
// ===================================================================

/// Static fallback buffer (used when no PSRAM buffer is set).
static SdLogRecord s_ring_buf_static[LOG_RING_SIZE_DEFAULT];

/// Active buffer pointer (redirected to PSRAM buffer when available).
static SdLogRecord* s_ring_buf = s_ring_buf_static;

/// Active buffer capacity (power of 2).
static uint32_t s_ring_capacity = LOG_RING_SIZE_DEFAULT;

/// Active buffer mask (capacity - 1) for fast wrap-around.
static uint32_t s_ring_mask = LOG_RING_SIZE_DEFAULT - 1;

/// Write index – only modified by the producer (control loop).
/// After writing to s_ring_buf[s_write_idx], the producer does:
///   s_write_idx = (s_write_idx + 1) & s_ring_mask;
static volatile uint32_t s_write_idx = 0;

/// Read index – only modified by the consumer (task_slow).
/// After reading from s_ring_buf[s_read_idx], the consumer does:
///   s_read_idx = (s_read_idx + 1) & s_ring_mask;
static volatile uint32_t s_read_idx = 0;

/// Overflow counter – incremented when the producer overwrites
/// data that the consumer hasn't read yet.
static volatile uint32_t s_overflow_count = 0;

/// Total records flushed to SD (incremented by consumer).
static volatile uint32_t s_records_flushed = 0;

/// Call counter for subsampling – incremented each time
/// sdLoggerRecord() is called. Only records when the counter
/// is a multiple of LOG_RATE_DIVIDER.
static volatile uint32_t s_call_counter = 0;

// ===================================================================
// Platform hooks – implemented in sd_logger_esp32.cpp
// ===================================================================

/// Read the hardware logging switch.
/// Returns true if logging should be active (pin LOW).
extern "C" bool sdLoggerReadSwitch(void);

/// Drain the ring buffer: read all pending records and write to SD.
/// Called by task_slow. Returns the number of records flushed.
extern "C" uint32_t sdLoggerDrainBuffer(void);

/// Close the current log file and release the SD card.
/// Called when the switch is turned OFF.
extern "C" void sdLoggerCloseFile(void);

// ===================================================================
// External buffer support – TASK-029
// ===================================================================

/// Set an external ring buffer (e.g. PSRAM-allocated).
///
/// After calling this, all ring buffer operations use the provided
/// buffer instead of the static fallback. The existing indices are
/// preserved (both producer and consumer pointers wrap at the new
/// capacity boundary).
///
/// @param buf       Pointer to the ring buffer (must be non-null).
/// @param capacity  Buffer capacity (must be power of 2, non-zero).
void sdLoggerSetExternalBuffer(SdLogRecord* buf, uint32_t capacity) {
    if (!buf || capacity == 0) return;
    if (capacity & (capacity - 1)) return;  // not power of 2

    s_ring_buf = buf;
    s_ring_capacity = capacity;
    s_ring_mask = capacity - 1;

    // Clamp indices to new capacity (preserve as many records as possible).
    s_write_idx = s_write_idx & s_ring_mask;
    s_read_idx  = s_read_idx  & s_ring_mask;
}

// ===================================================================
// Public API – platform-independent
// ===================================================================

void sdLoggerRecord(void) {
    // Only record every N-th call (subsample from 200 Hz to 10 Hz)
    uint32_t count = s_call_counter;
    s_call_counter = count + 1;

    if ((count % LOG_RATE_DIVIDER) != 0) {
        return;  // skip – not our turn
    }

    // Only buffer if logging is active (fast GPIO check)
    if (!sdLoggerReadSwitch()) {
        return;
    }

    // Capture current state into a temporary record.
    // We read the state directly from g_nav to avoid
    // holding the mutex in the control loop.
    extern NavigationState g_nav;

    SdLogRecord rec;
    {
        StateLock lock;
        rec.timestamp_ms     = g_nav.imu.imu_timestamp_ms;
        rec.heading_deg      = g_nav.imu.heading_deg;
        rec.steer_angle_deg  = g_nav.steer.steer_angle_deg;
        rec.desired_angle_deg = g_nav.sw.desiredSteerAngleDeg;  // ADR-STATE-001: jetzt in SwitchState
        rec.yaw_rate_dps     = g_nav.imu.yaw_rate_dps;
        rec.roll_deg         = g_nav.imu.roll_deg;
        rec.safety_ok        = g_nav.safety.safety_ok ? 1 : 0;
    }

    // Write to ring buffer (no mutex needed – SPSC pattern)
    uint32_t widx = s_write_idx;
    uint32_t next_widx = (widx + 1) & s_ring_mask;

    // Check for overflow (consumer hasn't caught up)
    if (next_widx == s_read_idx) {
        // Buffer full – advance read pointer (drop oldest)
        s_read_idx = (s_read_idx + 1) & s_ring_mask;
        s_overflow_count++;
    }

    s_ring_buf[widx] = rec;
    s_write_idx = next_widx;
}

bool sdLoggerIsActive(void) {
    return sdLoggerReadSwitch();
}

uint32_t sdLoggerGetRecordsFlushed(void) {
    return s_records_flushed;
}

uint32_t sdLoggerGetBufferCount(void) {
    uint32_t w = s_write_idx;
    uint32_t r = s_read_idx;
    if (w >= r) return w - r;
    return s_ring_capacity - r + w;
}

// ===================================================================
// TASK-029: PSRAM buffer diagnostics
// ===================================================================

bool sdLoggerPsramBufferActive(void) {
    return s_ring_buf != s_ring_buf_static;
}

uint32_t sdLoggerPsramBufferCount(void) {
    if (s_ring_buf == s_ring_buf_static) return 0;
    return sdLoggerGetBufferCount();
}

// ===================================================================
// Buffer access – called from task_slow (consumer side)
// ===================================================================

/// Check if there are records waiting in the ring buffer.
extern "C" bool sdLoggerHasRecords(void) {
    return s_write_idx != s_read_idx;
}

/// Read one record from the ring buffer (consumer side).
/// Returns true if a record was read, false if buffer is empty.
extern "C" bool sdLoggerReadRecord(SdLogRecord* out) {
    uint32_t r = s_read_idx;
    if (r == s_write_idx) {
        return false;  // buffer empty
    }

    *out = s_ring_buf[r];
    s_read_idx = (r + 1) & s_ring_mask;
    return true;
}

/// Increment the flushed counter (called by the consumer after writing to SD).
extern "C" void sdLoggerIncrementFlushed(uint32_t count) {
    s_records_flushed += count;
}

/// Get the overflow counter (for diagnostics).
extern "C" uint32_t sdLoggerGetOverflowCount(void) {
    return s_overflow_count;
}
