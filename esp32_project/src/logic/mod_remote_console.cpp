/**
 * @file mod_remote_console.cpp
 * @brief Infrastructure module: TCP/Telnet Remote Console (ModuleId::REMOTE_CONSOLE).
 *
 * Wraps the DebugConsole (DBG) TCP/Telnet server as a proper module.
 *
 * Dependencies: ETH (needs network interface for TCP bind).
 * Pipeline:     No-Op (infrastructure module, no data processing).
 * Config:       TCP port (NVS-persisted, default 23).
 *
 * Activate:   DBG.begin(port), enableTcp(true), setInputCallback()
 * Deactivate: DBG.end()
 * loop():     DBG.loop() — called from main loop() via output() pipeline
 */

#include "mod_remote_console.h"

#if FEAT_ENABLED(FEAT_COMPILED_REMOTE_CONSOLE)

#include "hal/hal.h"
#include "nvs_config.h"
#include "debug/DebugConsole.h"
#include <nvs.h>

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MOD
#include "esp_log.h"
#include "log_ext.h"

#include "cli.h"

#include <cstring>
#include <cstdio>

extern Stream* s_cli_out;

static const char* const TAG = "MOD_RCON";

// ===================================================================
// Error codes
// ===================================================================
static constexpr int32_t ERR_NOT_COMPILED   = 1;
static constexpr int32_t ERR_NO_NETWORK     = 2;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;
static ModuleRuntime* s_rt = nullptr;

// ===================================================================
// Config keys (NVS, namespace "agsteer")
// ===================================================================
namespace rcon_keys {
    static constexpr const char* PORT = "mod_rcon_port";
}

// Module-local config cache
static struct {
    uint16_t port  = 23;
    bool     dirty = false;
} s_cfg;

// ===================================================================
// Dependencies: needs ETH for TCP
// ===================================================================
static const ModuleId s_deps[] = { ModuleId::ETH, ModuleId::COUNT };

// ===================================================================
// NVS helpers
// ===================================================================
static bool rconNvsLoad(void) {
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &h) != ESP_OK) return false;

    uint16_t val = 23;
    if (nvs_get_u16(h, rcon_keys::PORT, &val) == ESP_OK) {
        s_cfg.port = (val == 0) ? 23 : val;  // port 0 → fallback to 23
    }
    s_cfg.dirty = false;
    nvs_close(h);
    return true;
}

static bool rconNvsSave(void) {
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READWRITE, &h) != ESP_OK) return false;

    bool ok = true;
    if (nvs_set_u16(h, rcon_keys::PORT, s_cfg.port) != ESP_OK) {
        ok = false;
        LOGW(TAG, "NVS set '%s' failed", rcon_keys::PORT);
    }

    esp_err_t commit_err = nvs_commit(h);
    if (commit_err != ESP_OK) {
        ok = false;
        LOGW(TAG, "NVS commit failed");
    }
    nvs_close(h);
    s_cfg.dirty = false;
    return ok && (commit_err == ESP_OK);
}

// ===================================================================
// Lifecycle
// ===================================================================
static bool rcon_is_enabled(void) {
    return feat::remote_console();
}

static void rcon_activate(void) {
    s_rt = moduleSysGet(ModuleId::REMOTE_CONSOLE);
    if (!s_rt) {
        LOGE(TAG, "moduleSysGet(REMOTE_CONSOLE) returned nullptr");
        return;
    }

    rconNvsLoad();

    DBG.begin(s_cfg.port);
    DBG.enableTcp(true);
    DBG.setInputCallback([](uint8_t c) {
        // TCP client input -> same CLI processing as Serial input
        static char s_tcp_buf[128];
        static size_t s_tcp_len = 0;

        if (c == '\r' || c == '\n') {
            if (s_tcp_len > 0) {
                s_tcp_buf[s_tcp_len] = '\0';
                DBG.println();
                cliProcessLine(s_tcp_buf);
                s_tcp_len = 0;
            }
        } else if (c == 3) { /* Ctrl+C */
            s_tcp_len = 0;
            DBG.println("^C");
        } else if (c == 8 || c == 127) {  // Backspace / DEL
            if (s_tcp_len > 0) {
                s_tcp_len--;
                DBG.print("\b \b");
            }
        } else if (s_tcp_len + 1 < sizeof(s_tcp_buf)) {
            s_tcp_buf[s_tcp_len++] = static_cast<char>(c);
            DBG.print(static_cast<char>(c));
        }
    });

    s_state.detected = true;
    s_state.error_code = 0;
    s_state.last_update_ms = hal_millis();

    char ip_buf[20] = {0};
    if (hal_net_is_connected()) {
        uint32_t ip = hal_net_get_ip();
        LOGI(TAG, "activated — TCP console on %u.%u.%u.%u:%u",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
             s_cfg.port);
    } else {
        LOGI(TAG, "activated — TCP console on port %u (waiting for IP...)", s_cfg.port);
    }

    if (s_rt) s_rt->state = s_state;
}

static void rcon_deactivate(void) {
    DBG.end();
    s_state.detected = false;
    s_state.quality_ok = false;
    s_state.error_code = 0;
    LOGI(TAG, "deactivated — TCP console stopped");
    if (s_rt) s_rt->state = s_state;
}

static bool rcon_is_healthy(uint32_t now_ms) {
    if (!s_state.detected) {
        s_state.error_code = ERR_NOT_COMPILED;
        s_state.quality_ok = false;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

    // Healthy if we have a network interface (TCP bind is deferred)
    if (!hal_net_is_connected()) {
        s_state.quality_ok = false;
        s_state.error_code = ERR_NO_NETWORK;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

    s_state.quality_ok = true;
    s_state.error_code = 0;
    s_state.last_update_ms = now_ms;
    if (s_rt) s_rt->state = s_state;
    return true;
}

// ===================================================================
// Pipeline — output() calls DBG.loop() for TCP accept/input/disconnect
// ===================================================================
static ModuleResult rcon_input(uint32_t /*now_ms*/) {
    return MOD_OK;
}

static ModuleResult rcon_process(uint32_t /*now_ms*/) {
    return MOD_OK;
}

static ModuleResult rcon_output(uint32_t /*now_ms*/) {
    // DBG.loop() handles: deferred bind, accept, read input, disconnect detection
    DBG.loop();
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================
static constexpr CfgKeyDef s_rcon_keys[] = {
    {"port", "TCP port for remote console (default: 23)"},
    {nullptr, nullptr}
};

static const CfgKeyDef* rcon_cfg_keys(void) { return s_rcon_keys; }

static bool rcon_cfg_get(const char* key, char* buf, size_t len) {
    if (!key || !buf || len == 0) return false;

    if (std::strcmp(key, "port") == 0) {
        std::snprintf(buf, len, "%u", s_cfg.port);
        return true;
    }
    return false;
}

static bool rcon_cfg_set(const char* key, const char* val) {
    if (!key || !val) return false;

    if (std::strcmp(key, "port") == 0) {
        int p = std::atoi(val);
        if (p < 1 || p > 65535) {
            LOGW(TAG, "cfg_set: port out of range (1-65535): %s", val);
            return false;
        }
        s_cfg.port = static_cast<uint16_t>(p);
        s_cfg.dirty = true;
        LOGI(TAG, "cfg_set: port = %u", s_cfg.port);
        return true;
    }
    return false;
}

static bool rcon_cfg_apply(void) {
    // Requires deactivate/reactivate to take effect (new port)
    LOGI(TAG, "cfg_apply: port=%u (requires re-activate)", s_cfg.port);
    return true;
}

static bool rcon_cfg_save(void) {
    return rconNvsSave();
}

static bool rcon_cfg_load(void) {
    return rconNvsLoad();
}

static bool rcon_cfg_show(void) {
    LOGI(TAG, "config: port=%u", s_cfg.port);
    LOGI(TAG, "  compiled=%d active=%d connected=%d",
         feat::remote_console() ? 1 : 0,
         moduleSysIsActive(ModuleId::REMOTE_CONSOLE) ? 1 : 0,
         DBG.isTcpClientConnected() ? 1 : 0);
    return true;
}

// ===================================================================
// Diagnostics
// ===================================================================
static void mod_rcon_diag_info(void) {
    if (!s_state.detected) {
        s_cli_out->printf("  Reason:    Remote console not active\r\n");
    } else if (s_state.error_code == ERR_NO_NETWORK) {
        s_cli_out->printf("  Reason:    No network connection (waiting for IP)\r\n");
    } else {
        s_cli_out->printf("  Reason:    OK — port %u", s_cfg.port);
        if (DBG.isTcpClientConnected()) {
            s_cli_out->printf(" [client connected]\r\n");
        } else {
            s_cli_out->printf(" [no client]\r\n");
        }
    }
}

static bool rcon_debug(void) {
    s_cli_out->printf("=== Remote Console Debug ===\r\n");
    s_cli_out->printf("  Compiled:    %s\r\n", feat::remote_console() ? "yes" : "no");
    s_cli_out->printf("  Active:      %s\r\n", moduleSysIsActive(ModuleId::REMOTE_CONSOLE) ? "yes" : "no");
    s_cli_out->printf("  TCP enabled: %s\r\n", DBG.isTcpEnabled() ? "yes" : "no");
    s_cli_out->printf("  Port:        %u\r\n", DBG.getTcpPort());
    s_cli_out->printf("  Client:      %s\r\n", DBG.isTcpClientConnected() ? "CONNECTED" : "none");
    s_cli_out->printf("  Targets:     0x%02x\r\n", DBG.getTargets());
    s_cli_out->printf("  TX bytes:    %lu\r\n", (unsigned long)DBG.getTcpBytesWritten());
    s_cli_out->printf("  RX bytes:    %lu\r\n", (unsigned long)DBG.getTcpBytesRead());
    s_cli_out->printf("  Connects:    %lu\r\n", (unsigned long)DBG.getTcpConnectCount());
    s_cli_out->printf("  Drops:       %lu\r\n", (unsigned long)DBG.getTcpDropCount());
    return true;
}

// ===================================================================
// Ops table
// ===================================================================
const ModuleOps2 mod_remote_console_ops = {
    .name      = "REMOTE_CONSOLE",
    .id        = ModuleId::REMOTE_CONSOLE,

    .is_enabled  = rcon_is_enabled,
    .activate    = rcon_activate,
    .deactivate  = rcon_deactivate,
    .is_healthy  = rcon_is_healthy,

    .input   = rcon_input,
    .process = rcon_process,
    .output  = rcon_output,

    .cfg_keys   = rcon_cfg_keys,
    .cfg_get   = rcon_cfg_get,
    .cfg_set   = rcon_cfg_set,
    .cfg_apply = rcon_cfg_apply,
    .cfg_save  = rcon_cfg_save,
    .cfg_load  = rcon_cfg_load,
    .cfg_show  = rcon_cfg_show,

    .diag_info = mod_rcon_diag_info,
    .debug     = rcon_debug,

    .deps = s_deps,
};

#endif  // FEAT_ENABLED(FEAT_COMPILED_REMOTE_CONSOLE)
