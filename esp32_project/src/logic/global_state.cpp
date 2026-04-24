/**
 * @file global_state.cpp
 * @brief Global navigation state instance definition.
 */

#include "global_state.h"

#include <cstring>

/// Global navigation state – single source of truth
NavigationState g_nav = {};

// ADR-STATE-001: desiredSteerAngleDeg wurde in g_nav.sw.desiredSteerAngleDeg verschoben.

// ---------------------------------------------------------------------------
// NTRIP global state — TASK-025
// ---------------------------------------------------------------------------
/// Default NTRIP configuration (empty — must be configured before use).
NtripConfig g_ntrip_config = {};

/// NTRIP runtime state (initialised to IDLE).
NtripState g_ntrip = {
    NtripConnState::IDLE,   // conn_state
    0,                      // state_enter_ms
    0,                      // last_rtcm_ms
    0,                      // rx_bytes
    0,                      // forwarded_bytes
    0,                      // connect_failures
    0,                      // last_http_status
    {}                      // last_error
};
