/**
 * @file ntrip.h
 * @brief NTRIP client declarations — TASK-025.
 *
 * Procedural functions for NTRIP caster connection, RTCM stream
 * reception, and forwarding to GNSS receivers.
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

/// State machine tick — call from maintTask (TASK-029).
/// Handles state transitions, reconnect timing, and error recovery.
/// Blocking TCP connect runs here (safe at lowest priority).
void ntripTick(void);

// ===================================================================
// NTRIP data flow
// ===================================================================

/// Read available RTCM data from the NTRIP TCP stream into
/// g_ntrip.rtcm_buf. Call from commTask input phase.
void ntripReadRtcm(void);

/// Forward buffered RTCM data to all GNSS receivers with
/// rtcm_source = LOCAL. Call from commTask output phase.
void ntripForwardRtcm(void);

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
