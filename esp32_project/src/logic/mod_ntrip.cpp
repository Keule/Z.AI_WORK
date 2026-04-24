/**
 * @file mod_ntrip.cpp
 * @brief NTRIP client module implementation.
 *
 * Migrated from ntrip.cpp. Wraps existing NTRIP state machine and
 * data flow functions into the ModuleOps2 interface.
 * Error codes: 1=connect_failed, 2=auth_failed, 3=disconnected
 */

#include "mod_ntrip.h"

#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)

#include "ntrip.h"
#include "global_state.h"
#include "hal/hal.h"
#include "dependency_policy.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_NTRIP
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>
#include <cstring>

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
    // Read RTCM data from TCP stream into ring buffer
    ntripReadRtcm();

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
    // connect (5s timeout) and must run in the low-priority maintTask
    // (TASK-029/ADR-002). The maintTask calls ntripTick() directly.

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
    // Forward RTCM data from ring buffer to GNSS receivers
    ntripForwardRtcm();
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
// Debug
// ===================================================================

static bool mod_ntrip_debug(void) {
    NtripState snap;
    {
        StateLock lock;
        snap = g_ntrip;
    }

    LOGI("NTRIP", "debug: state=%d rx=%lu fwd=%lu http=%u err=\"%s\"",
         static_cast<int>(snap.conn_state),
         static_cast<unsigned long>(snap.rx_bytes),
         static_cast<unsigned long>(snap.forwarded_bytes),
         static_cast<unsigned>(snap.last_http_status),
         snap.last_error[0] ? snap.last_error : "(none)");
    return (snap.conn_state == NtripConnState::CONNECTED);
}

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
    /* cfg_get     */ mod_ntrip_cfg_get,
    /* cfg_set     */ mod_ntrip_cfg_set,
    /* cfg_apply   */ mod_ntrip_cfg_apply,
    /* cfg_save    */ mod_ntrip_cfg_save,
    /* cfg_load    */ mod_ntrip_cfg_load,
    /* cfg_show    */ mod_ntrip_cfg_show,
    /* debug       */ mod_ntrip_debug,
    /* deps        */ s_deps
};

#endif // FEAT_ENABLED(FEAT_COMPILED_NTRIP)
