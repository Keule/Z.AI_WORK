/**
 * @file shared_state.h
 * @brief Shared data slot for cross-task communication (ADR-007).
 *
 * Provides SharedSlot<T>: a typed, metadata-rich container for data
 * flowing from task_slow (or sub-tasks) to task_fast.
 *
 * All accesses MUST be protected by StateLock (ADR-STATE-001).
 *
 * Usage pattern (producer — task_slow / sub-task):
 *   StateLock lock;
 *   slot.data = ...;
 *   slot.last_update_ms = hal_millis();
 *   slot.dirty = true;
 *   slot.source_id = static_cast<uint8_t>(ModuleId::MY_MODULE);
 *
 * Usage pattern (consumer — task_fast, in module input()):
 *   StateLock lock;
 *   if (slot.dirty && (now_ms - slot.last_update_ms < FRESHNESS_MS)) {
 *       auto local_copy = slot.data;  // copy under lock
 *       slot.dirty = false;
 *       // process local_copy outside lock if needed
 *   }
 *   // else: data stale → skip or use last-known-good
 */

#pragma once

#include <cstdint>

/**
 * Typed shared data slot for safe cross-task data exchange.
 *
 * @tparam T  The data type stored in this slot. Must be trivially copyable.
 */
template<typename T>
struct SharedSlot {
    T        data {};               ///< Payload data
    uint32_t last_update_ms = 0;    ///< Timestamp of last write (hal_millis())
    bool     dirty = false;         ///< true = unread new data available
    uint8_t  source_id = 0;         ///< Writer identifier (ModuleId cast)
};

/// Error codes for SharedSlot operations (returned by modules).
enum class SharedSlotError : uint32_t {
    OK       = 0,
    STALE    = 1,   ///< Data older than freshness threshold
    NOT_DIRTY = 2,  ///< No new data since last read
    EMPTY    = 3,   ///< Slot has never been written
};
