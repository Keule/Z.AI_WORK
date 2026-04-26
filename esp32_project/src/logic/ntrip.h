/**
 * @file ntrip.h
 * @brief NTRIP client declarations — TASK-025, Phase 3 SharedSlot integration.
 *
 * Procedural functions for NTRIP caster connection and RTCM stream
 * reception. RTCM data flows through SharedSlot<RtcmChunk> (ADR-007 §2.5):
 *
 *   task_slow (producer):  ntripTick() → ntripReadToSlot() → g_rtcm_slot
 *   task_fast (consumer):  mod_ntrip_input() reads g_rtcm_slot → GNSS UART
 *
 * All NTRIP code is gated behind FEAT_NTRIP (set via -DFEAT_NTRIP).
 * Without the flag, no NTRIP code is compiled (zero overhead).
 */

#pragma once

#include "features.h"

#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)

#include "global_state.h"
#include <cstddef>
#include <cstdint>

// ===================================================================
// NTRIP client lifecycle
// ===================================================================

/// One-time initialisation of the NTRIP client state.
/// Must be called after hal_mutex_init() and before ntripTick().
void ntripInit(void);

/// State machine tick — call from task_slow (TASK-029).
/// Handles state transitions, reconnect timing, and error recovery.
/// Blocking TCP connect runs here (safe on Core 0 slow path).
void ntripTick(void);

// ===================================================================
// NTRIP data flow (Phase 3: SharedSlot-based)
// ===================================================================

/// Read available RTCM data from the NTRIP TCP stream and store in
/// g_rtcm_slot (SharedSlot<RtcmChunk>). Call from task_slow every
/// poll cycle (~100 Hz) when NTRIP module is active.
///
/// Non-blocking: reads whatever is available from TCP, writes at
/// most RTCM_SLOT_SIZE bytes into the shared slot, and sets dirty=true.
/// If no data is available or not in CONNECTED state, returns immediately.
void ntripReadToSlot(void);

// ===================================================================
// NTRIP configuration helpers
// ===================================================================

/// Set the NTRIP caster connection parameters.
/// Copies the strings into g_ntrip_config (thread-safe via StateLock).
void ntripSetConfig(const char* host, uint16_t port,
                    const char* mountpoint,
                    const char* user, const char* password);

/// Set the NTRIP reconnect delay in milliseconds.
void ntripSetReconnectDelay(uint32_t delay_ms);

/// Get a snapshot of the current NTRIP state (thread-safe).
NtripState ntripGetState(void);

#endif // FEAT_ENABLED(FEAT_COMPILED_NTRIP)
