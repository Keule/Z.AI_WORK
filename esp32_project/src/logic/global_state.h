/**
 * @file global_state.h
 * @brief Global navigation and vehicle state, shared between tasks.
 *
 * Pure C++ – no Arduino / ESP32 headers.
 * All accesses should be protected with the mutex helpers provided.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "state_structs.h"
#include "shared_state.h"

// ---------------------------------------------------------------------------
// Mutex abstraction – implemented in HAL
// ---------------------------------------------------------------------------
extern "C" {
    /// Create / init the global state mutex. Called once during setup.
    void  hal_mutex_init(void);

    /// Lock the global state mutex.
    void  hal_mutex_lock(void);

    /// Unlock the global state mutex.
    void  hal_mutex_unlock(void);
}

// ---------------------------------------------------------------------------
// Navigation state – single source of truth for control + comms tasks.
// ---------------------------------------------------------------------------
struct NavigationState {
    ImuState       imu;
    SteerState     steer;
    SwitchState    sw;
    PidConfigState pid;
    SafetyState    safety;
    GnssState      gnss;
};

// ---------------------------------------------------------------------------
// Global instance (defined in control.cpp or main)
// ---------------------------------------------------------------------------
extern NavigationState g_nav;

// ---------------------------------------------------------------------------
// ADR-STATE-001: desiredSteerAngleDeg wurde in SwitchState verschoben.
// Alle Zugriffe muessen StateLock verwenden.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// RAII helper: lock mutex in ctor, unlock in dtor.
// ---------------------------------------------------------------------------
class StateLock {
public:
    StateLock()  { hal_mutex_lock(); }
    ~StateLock() { hal_mutex_unlock(); }
};

// ---------------------------------------------------------------------------
// NTRIP state — TASK-025
// ---------------------------------------------------------------------------
/// NTRIP connection state machine states.
enum class NtripConnState : uint8_t {
    IDLE           = 0,   ///< Not started / idle
    CONNECTING     = 1,   ///< TCP connect in progress
    AUTHENTICATING = 2,   ///< HTTP request sent, waiting for response
    CONNECTED      = 3,   ///< Connected, receiving RTCM stream
    ERROR          = 4,   ///< Error state, will transition to IDLE for reconnect
    DISCONNECTED   = 5    ///< Clean disconnect, will reconnect after delay
};

/// NTRIP client configuration.
struct NtripConfig {
    char     host[64];          ///< NTRIP caster hostname or IP
    uint16_t port;              ///< NTRIP caster port
    char     mountpoint[48];    ///< NTRIP mountpoint (e.g. "VRS")
    char     user[32];          ///< NTRIP username (empty = no auth)
    char     password[32];      ///< NTRIP password
    uint32_t reconnect_delay_ms; ///< Delay before reconnect after error (default 5000)
};

/// NTRIP client runtime state and statistics.
struct NtripState {
    NtripConnState conn_state;       ///< Current state machine state
    uint32_t       state_enter_ms;   ///< millis() when current state was entered
    uint32_t       last_rtcm_ms;     ///< millis() of last RTCM data received
    uint32_t       rx_bytes;         ///< Total RTCM bytes received from caster
    uint32_t       forwarded_bytes;  ///< Total RTCM bytes forwarded to GNSS receivers
    uint32_t       connect_failures; ///< Count of failed connection attempts
    uint8_t        last_http_status; ///< Last HTTP response code (0 = no response)
    char           last_error[64];   ///< Last error description (empty = none)
};

/// GNSS receiver RTCM source type.
enum class RtcmSource : uint8_t {
    LOCAL = 0,   ///< Gets RTCM from local NTRIP client
    OWN   = 1    ///< Gets RTCM independently (e.g. remote receiver)
};

/// GNSS receiver configuration (compile-time, board-specific).
struct GnssRxConfig {
    uint8_t    uart_inst;        ///< UART instance (0=UART0, 1=UART1, 2=UART2)
    int8_t     tx_pin;           ///< UART TX GPIO
    int8_t     rx_pin;           ///< UART RX GPIO (-1 = unbound)
    uint32_t   baud;             ///< UART baud rate
    RtcmSource rtcm_source;      ///< Where this receiver gets RTCM from
};

/// Per-receiver runtime state.
struct GnssRxState {
    bool     ready;              ///< true after successful hal_gnss_uart_begin()
    uint32_t forwarded_bytes;    ///< Total RTCM bytes forwarded to this receiver
    uint32_t drop_count;         ///< Total bytes dropped (UART buffer full)
};

// ---------------------------------------------------------------------------
// RTCM SharedSlot — Phase 3 (ADR-007 §2.5)
// ---------------------------------------------------------------------------

/// Maximum bytes per RTCM shared-slot chunk.
/// Matches the TCP read buffer size — single chunk per producer cycle.
static constexpr size_t RTCM_SLOT_SIZE = 512;

/// RTCM data chunk for SharedSlot cross-task transport.
/// Producer (task_slow / ntripReadToSlot) writes; consumer (task_fast /
/// mod_ntrip_input) reads. Trivially copyable for safe memcpy under StateLock.
struct RtcmChunk {
    uint8_t  data[RTCM_SLOT_SIZE];  ///< Raw RTCM3 bytes
    uint16_t len;                    ///< Number of valid bytes in data[]
};

/// SharedSlot for RTCM data flow: task_slow → task_fast.
/// All accesses MUST be protected by StateLock (ADR-STATE-001).
extern SharedSlot<RtcmChunk> g_rtcm_slot;

// ---------------------------------------------------------------------------
// Global instances — TASK-025
// ---------------------------------------------------------------------------
extern NtripConfig g_ntrip_config;
extern NtripState  g_ntrip;

// ---------------------------------------------------------------------------
// Scoped lock for arbitrary lock/unlock function pointers.
// ---------------------------------------------------------------------------
template <void (*LockFn)(), void (*UnlockFn)()>
class ScopedLock {
public:
    ScopedLock()  { LockFn(); }
    ~ScopedLock() { UnlockFn(); }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
};
