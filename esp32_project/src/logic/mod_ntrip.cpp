/**
 * @file mod_ntrip.cpp
 * @brief NTRIP client module implementation — Phase 3 SharedSlot.
 *
 * Wraps the NTRIP state machine and data flow into the ModuleOps2
 * interface. RTCM data flows through SharedSlot<RtcmChunk> (ADR-007 §2.5):
 *
 *   task_slow (producer):  ntripTick() → ntripReadToSlot() → g_rtcm_slot
 *   task_fast (consumer):  mod_ntrip_input() reads g_rtcm_slot → GNSS UART
 *
 * Error codes: 1=connect_failed, 2=auth_failed, 3=disconnected
 */

#include "mod_ntrip.h"

#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)

#include "ntrip.h"
#include "global_state.h"
#include "hal/hal.h"
#include "dependency_policy.h"
#include "cli.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_NTRIP
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>
#include <cstring>

extern Stream* s_cli_out;

// ===================================================================
// Freshness timeout
// ===================================================================
static constexpr uint32_t FRESHNESS_TIMEOUT_MS = dep_policy::NTRIP_RTCM_FRESHNESS_TIMEOUT_MS;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;

// ===================================================================
// Dependencies: requires NETWORK (for TCP) and GNSS (for RTCM forwarding)
// ===================================================================
static const ModuleId s_deps[] = {
    ModuleId::NETWORK,
    ModuleId::GNSS,
    ModuleId::COUNT   // nullptr-terminated via sentinel
};

// ===================================================================
// Lifecycle
// ===================================================================

static bool mod_ntrip_is_enabled(void) {
    return feat::ntrip();
}

static void mod_ntrip_activate(void) {
    s_state = {};
    ntripInit();
    s_state.detected = true;  // NTRIP is a software module, always "detected"
    LOGI("NTRIP", "module activated");
}

static void mod_ntrip_deactivate(void) {
    // Disconnect from NTRIP caster
    {
        StateLock lock;
        if (g_ntrip.conn_state == NtripConnState::CONNECTED) {
            hal_tcp_disconnect();
        }
        g_ntrip.conn_state = NtripConnState::IDLE;
    }
    s_state = {};
    LOGI("NTRIP", "module deactivated");
}

static bool mod_ntrip_is_healthy(uint32_t now_ms) {
    NtripConnState conn_state;
    uint32_t last_rtcm_ms;
    {
        StateLock lock;
        conn_state = g_ntrip.conn_state;
        last_rtcm_ms = g_ntrip.last_rtcm_ms;
    }

    // Must be connected
    if (conn_state != NtripConnState::CONNECTED) {
        s_state.error_code = 3;  // disconnected
        return false;
    }

    // Must have fresh RTCM data
    if (last_rtcm_ms > 0 &&
        (now_ms - last_rtcm_ms) > FRESHNESS_TIMEOUT_MS) {
        s_state.error_code = 3;
        return false;
    }

    if (s_state.error_code != 0) {
        return false;
    }

    return true;
}

// ===================================================================
// Pipeline
// ===================================================================

static ModuleResult mod_ntrip_input(uint32_t now_ms) {
    // Phase 3: Read RTCM data from SharedSlot instead of TCP.
    // Producer (task_slow / ntripReadToSlot) writes to g_rtcm_slot;
    // consumer (task_fast / mod_ntrip_input) reads and forwards.
    //
    // Consumer pattern (ADR-007 §2.5):
    //   StateLock lock;
    //   if (slot.dirty && fresh) { copy, dirty=false; process; }

    // Quick check without lock — dirty is only set true by producer
    // and cleared by us. A stale read of false just means we skip.
    if (!g_rtcm_slot.dirty) {
        // Update freshness tracking even when no new data
        {
            StateLock lock;
            if (g_ntrip.conn_state == NtripConnState::CONNECTED) {
                s_state.last_update_ms = now_ms;
                s_state.quality_ok = (g_ntrip.last_rtcm_ms > 0);
            }
        }
        return MOD_OK;
    }

    // Read SharedSlot under lock — consumer pattern
    RtcmChunk local_chunk;
    bool had_data = false;
    {
        StateLock lock;
        if (g_rtcm_slot.dirty &&
            g_rtcm_slot.data.len > 0 &&
            (now_ms - g_rtcm_slot.last_update_ms < FRESHNESS_TIMEOUT_MS)) {
            local_chunk = g_rtcm_slot.data;  // copy under lock
            g_rtcm_slot.dirty = false;       // consume
            had_data = true;
        } else {
            // Stale or empty — clear dirty flag
            g_rtcm_slot.dirty = false;
        }
    }

    if (had_data) {
        // Forward RTCM data to ALL GNSS receivers with rtcm_source = LOCAL.
        // Note: SharedSlot ensures ALL receivers see the SAME data,
        // fixing the former ring-buffer pop-first issue (ADR-NTRIP-002).
        for (uint8_t inst = 0; inst < GNSS_RX_MAX; inst++) {
            if (!hal_gnss_uart_is_ready(inst)) continue;
            const size_t accepted = hal_gnss_uart_write(
                inst, local_chunk.data, local_chunk.len);
            if (accepted < local_chunk.len) {
                LOGW("NTRIP", "UART%d partial write: %u/%u bytes",
                     (unsigned)inst,
                     (unsigned)accepted, (unsigned)local_chunk.len);
            }
        }

        // Update forwarded statistics
        {
            StateLock lock;
            g_ntrip.forwarded_bytes += local_chunk.len;
        }
    }

    // Update freshness tracking
    {
        StateLock lock;
        if (g_ntrip.conn_state == NtripConnState::CONNECTED) {
            s_state.last_update_ms = now_ms;
            s_state.quality_ok = (g_ntrip.last_rtcm_ms > 0);
        }
    }

    return MOD_OK;
}

static ModuleResult mod_ntrip_process(uint32_t now_ms) {
    // NOTE: ntripTick() is NOT called here — it contains a blocking TCP
    // connect (5s timeout) and must run in task_slow
    // (TASK-029/ADR-002). task_slow calls ntripTick() directly.

    // Check for error states and map to error codes
    NtripConnState conn_state;
    {
        StateLock lock;
        conn_state = g_ntrip.conn_state;
    }

    switch (conn_state) {
    case NtripConnState::CONNECTED:
        s_state.error_code = 0;
        break;
    case NtripConnState::ERROR: {
        // Determine error type from last_http_status
        uint8_t http_status;
        char last_error[64];
        {
            StateLock lock;
            http_status = g_ntrip.last_http_status;
            std::strncpy(last_error, g_ntrip.last_error, sizeof(last_error) - 1);
            last_error[sizeof(last_error) - 1] = '\0';
        }
        if (http_status == 401) {
            s_state.error_code = 2;  // auth_failed
        } else if (std::strstr(last_error, "connect") != nullptr ||
                   std::strstr(last_error, "TCP") != nullptr) {
            s_state.error_code = 1;  // connect_failed
        } else {
            s_state.error_code = 1;  // generic connect/error
        }
        break;
    }
    case NtripConnState::DISCONNECTED:
        s_state.error_code = 3;
        break;
    default:
        // IDLE, CONNECTING, AUTHENTICATING — not an error yet
        break;
    }

    (void)now_ms;
    return MOD_OK;
}

static ModuleResult mod_ntrip_output(uint32_t /*now_ms*/) {
    // Phase 3: RTCM forwarding moved to input() (SharedSlot consumer).
    // Output is now a no-op — all GNSS forwarding happens in input().
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================

static bool mod_ntrip_cfg_get(const char* key, char* buf, size_t len) {
    StateLock lock;
    if (std::strcmp(key, "host") == 0) {
        std::strncpy(buf, g_ntrip_config.host, len - 1);
        buf[len - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "port") == 0) {
        std::snprintf(buf, len, "%u", static_cast<unsigned>(g_ntrip_config.port));
        return true;
    }
    if (std::strcmp(key, "mountpoint") == 0) {
        std::strncpy(buf, g_ntrip_config.mountpoint, len - 1);
        buf[len - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "user") == 0) {
        std::strncpy(buf, g_ntrip_config.user, len - 1);
        buf[len - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "password") == 0) {
        // Mask password in output for security
        std::strncpy(buf, "********", len - 1);
        buf[len - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "reconnect_ms") == 0) {
        std::snprintf(buf, len, "%lu",
                      static_cast<unsigned long>(g_ntrip_config.reconnect_delay_ms));
        return true;
    }
    return false;
}

static bool mod_ntrip_cfg_set(const char* key, const char* val) {
    if (std::strcmp(key, "host") == 0) {
        ntripSetConfig(val, 0, nullptr, nullptr, nullptr);
        // Re-read port to preserve it
        return true;
    }
    if (std::strcmp(key, "port") == 0) {
        uint16_t port = static_cast<uint16_t>(std::strtoul(val, nullptr, 10));
        StateLock lock;
        g_ntrip_config.port = port;
        return true;
    }
    if (std::strcmp(key, "mountpoint") == 0) {
        StateLock lock;
        std::strncpy(g_ntrip_config.mountpoint, val,
                     sizeof(g_ntrip_config.mountpoint) - 1);
        g_ntrip_config.mountpoint[sizeof(g_ntrip_config.mountpoint) - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "user") == 0) {
        StateLock lock;
        std::strncpy(g_ntrip_config.user, val, sizeof(g_ntrip_config.user) - 1);
        g_ntrip_config.user[sizeof(g_ntrip_config.user) - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "password") == 0) {
        StateLock lock;
        std::strncpy(g_ntrip_config.password, val,
                     sizeof(g_ntrip_config.password) - 1);
        g_ntrip_config.password[sizeof(g_ntrip_config.password) - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "reconnect_ms") == 0) {
        uint32_t delay = static_cast<uint32_t>(std::strtoul(val, nullptr, 10));
        ntripSetReconnectDelay(delay);
        return true;
    }
    return false;
}

static bool mod_ntrip_cfg_apply(void) {
    // Config is applied immediately on set; no deferred apply needed.
    // Trigger a reconnect by resetting state to IDLE.
    {
        StateLock lock;
        if (g_ntrip.conn_state != NtripConnState::IDLE) {
            g_ntrip.conn_state = NtripConnState::IDLE;
            g_ntrip.state_enter_ms = hal_millis();
        }
    }
    return true;
}

static bool mod_ntrip_cfg_save(void) {
    // Future: persist g_ntrip_config to NVS
    return true;
}

static bool mod_ntrip_cfg_load(void) {
    // Future: load g_ntrip_config from NVS
    return true;
}

static bool mod_ntrip_cfg_show(void) {
    NtripState snap;
    {
        StateLock lock;
        snap = g_ntrip;
    }

    const char* state_str = "UNKNOWN";
    switch (snap.conn_state) {
    case NtripConnState::IDLE:           state_str = "IDLE"; break;
    case NtripConnState::CONNECTING:     state_str = "CONNECTING"; break;
    case NtripConnState::AUTHENTICATING: state_str = "AUTHENTICATING"; break;
    case NtripConnState::CONNECTED:      state_str = "CONNECTED"; break;
    case NtripConnState::ERROR:          state_str = "ERROR"; break;
    case NtripConnState::DISCONNECTED:   state_str = "DISCONNECTED"; break;
    }

    LOGI("NTRIP", "state=%s error=%ld rx=%lu fwd=%lu fails=%lu http=%u err=\"%s\"",
         state_str,
         static_cast<long>(s_state.error_code),
         static_cast<unsigned long>(snap.rx_bytes),
         static_cast<unsigned long>(snap.forwarded_bytes),
         static_cast<unsigned long>(snap.connect_failures),
         static_cast<unsigned>(snap.last_http_status),
         snap.last_error[0] ? snap.last_error : "(none)");
    return true;
}

// ===================================================================
// Diagnostics
// ===================================================================

static void mod_ntrip_diag_info(void) {
    NtripState snap;
    {
        StateLock lock;
        snap = g_ntrip;
    }

    const char* state_str = "UNKNOWN";
    switch (snap.conn_state) {
    case NtripConnState::IDLE:           state_str = "IDLE"; break;
    case NtripConnState::CONNECTING:     state_str = "CONNECTING"; break;
    case NtripConnState::AUTHENTICATING: state_str = "AUTHENTICATING"; break;
    case NtripConnState::CONNECTED:      state_str = "CONNECTED"; break;
    case NtripConnState::ERROR:          state_str = "ERROR"; break;
    case NtripConnState::DISCONNECTED:   state_str = "DISCONNECTED"; break;
    }

    const uint32_t now = hal_millis();

    if (snap.conn_state == NtripConnState::CONNECTED) {
        if (snap.last_rtcm_ms > 0 && (now - snap.last_rtcm_ms) > FRESHNESS_TIMEOUT_MS) {
            s_cli_out->printf("  Reason:    RTCM data stale (%lu ms ago, timeout %lu ms)\n",
                (unsigned long)(now - snap.last_rtcm_ms),
                (unsigned long)FRESHNESS_TIMEOUT_MS);
        } else if (snap.last_rtcm_ms == 0) {
            s_cli_out->printf("  Reason:    connected, but no RTCM data received yet\n");
        } else {
            s_cli_out->printf("  Reason:    connected, RTCM flowing (%lu ms ago)\n",
                (unsigned long)(now - snap.last_rtcm_ms));
        }
    } else if (snap.conn_state == NtripConnState::ERROR) {
        s_cli_out->printf("  Reason:    %s — HTTP %u, err=\"%s\"\n",
            state_str,
            static_cast<unsigned>(snap.last_http_status),
            snap.last_error[0] ? snap.last_error : "(none)");
    } else {
        s_cli_out->printf("  Reason:    %s — not connected\n", state_str);
    }
    s_cli_out->printf("  Status:    state=%s rx=%lu fwd=%lu fails=%u\n",
        state_str,
        static_cast<unsigned long>(snap.rx_bytes),
        static_cast<unsigned long>(snap.forwarded_bytes),
        static_cast<unsigned>(snap.connect_failures));
}

static bool mod_ntrip_debug(void) {
    NtripState snap;
    {
        StateLock lock;
        snap = g_ntrip;
    }

    const char* state_str = "UNKNOWN";
    switch (snap.conn_state) {
    case NtripConnState::IDLE:           state_str = "IDLE"; break;
    case NtripConnState::CONNECTING:     state_str = "CONNECTING"; break;
    case NtripConnState::AUTHENTICATING: state_str = "AUTHENTICATING"; break;
    case NtripConnState::CONNECTED:      state_str = "CONNECTED"; break;
    case NtripConnState::ERROR:          state_str = "ERROR"; break;
    case NtripConnState::DISCONNECTED:   state_str = "DISCONNECTED"; break;
    }

    const uint32_t now = hal_millis();
    s_cli_out->printf("  State:          %s\n", state_str);
    s_cli_out->printf("  Connected:      %s\n", snap.conn_state == NtripConnState::CONNECTED ? "YES" : "NO");
    s_cli_out->printf("  HTTP status:    %u\n", static_cast<unsigned>(snap.last_http_status));
    s_cli_out->printf("  Last error:     %s\n", snap.last_error[0] ? snap.last_error : "(none)");
    s_cli_out->printf("  RTCM rx:        %lu bytes total\n", static_cast<unsigned long>(snap.rx_bytes));
    s_cli_out->printf("  RTCM forwarded: %lu bytes total\n", static_cast<unsigned long>(snap.forwarded_bytes));
    s_cli_out->printf("  Last RTCM:      %lu ms ago\n", snap.last_rtcm_ms > 0 ? (unsigned long)(now - snap.last_rtcm_ms) : 0UL);
    s_cli_out->printf("  Connect fails:  %u\n", static_cast<unsigned>(snap.connect_failures));
    s_cli_out->printf("  State timeout:  %lu ms\n", (unsigned long)FRESHNESS_TIMEOUT_MS);
    s_cli_out->printf("  Host:           %s:%u\n", g_ntrip_config.host, static_cast<unsigned>(g_ntrip_config.port));
    s_cli_out->printf("  Mountpoint:     %s\n", g_ntrip_config.mountpoint);
    s_cli_out->printf("  User:           %s\n", g_ntrip_config.user);
    return (snap.conn_state == NtripConnState::CONNECTED);
}

// ===================================================================
// Config key definitions
// ===================================================================
static const CfgKeyDef s_ntrip_keys[] = {
    {"host",        "NTRIP caster hostname"},
    {"port",        "NTRIP caster port"},
    {"mountpoint",  "NTRIP mountpoint"},
    {"user",        "NTRIP username"},
    {"password",    "NTRIP password"},
    {"reconnect_ms", "Reconnect delay (ms)"},
    {nullptr, nullptr}  // sentinel
};

static const CfgKeyDef* mod_ntrip_cfg_keys(void) { return s_ntrip_keys; }

// ===================================================================
// Ops table
// ===================================================================

const ModuleOps2 mod_ntrip_ops = {
    /* name        */ "NTRIP",
    /* id          */ ModuleId::NTRIP,
    /* is_enabled  */ mod_ntrip_is_enabled,
    /* activate    */ mod_ntrip_activate,
    /* deactivate  */ mod_ntrip_deactivate,
    /* is_healthy  */ mod_ntrip_is_healthy,
    /* input       */ mod_ntrip_input,
    /* process     */ mod_ntrip_process,
    /* output      */ mod_ntrip_output,
    /* cfg_keys   */ mod_ntrip_cfg_keys,
    /* cfg_get     */ mod_ntrip_cfg_get,
    /* cfg_set     */ mod_ntrip_cfg_set,
    /* cfg_apply   */ mod_ntrip_cfg_apply,
    /* cfg_save    */ mod_ntrip_cfg_save,
    /* cfg_load    */ mod_ntrip_cfg_load,
    /* cfg_show    */ mod_ntrip_cfg_show,
    /* diag_info   */ mod_ntrip_diag_info,
    /* debug       */ mod_ntrip_debug,
    /* deps        */ s_deps
};

#endif // FEAT_ENABLED(FEAT_COMPILED_NTRIP)
