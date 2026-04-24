/**
 * @file mod_eth.cpp
 * @brief Transport module: W5500 Ethernet (ModuleId::ETH).
 *
 * Wraps hal_net_* functions into the ModuleOps2 interface.
 * Migrated from net.cpp Ethernet initialisation.
 *
 * Config parameters (NVS-persisted, namespace "agsteer"):
 *   mode    — "dhcp" (default) or "static"
 *   ip      — Static IPv4 as dotted-decimal string "192.168.1.70"
 *   gateway — Static gateway as dotted-decimal string "192.168.1.1"
 *   subnet  — Static subnet as dotted-decimal string "255.255.255.0"
 *   dns     — DNS server as dotted-decimal string "8.8.8.8"
 *
 * CLI usage:
 *   module ETH show        — Status + config overview
 *   module ETH set mode dhcp        — Switch to DHCP
 *   module ETH set mode static      — Switch to static mode
 *   module ETH set ip 192.168.1.70  — Set static IP (requires apply)
 *   module ETH set gw 192.168.1.1   — Set gateway
 *   module ETH set mask 255.255.255.0 — Set subnet mask
 *   module ETH set dns 8.8.8.8      — Set DNS server
 *   module ETH apply       — Restart ETH with new config
 *   module ETH save        — Persist to NVS
 *   module ETH debug       — Detailed diagnostics
 */

#include "mod_eth.h"
#include "hal/hal.h"
#include "nvs_config.h"
#include "cli.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_NET
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <nvs.h>
#endif

static const char* const TAG = "MOD_ETH";

// ===================================================================
// Error codes
// ===================================================================
static constexpr int32_t ERR_NOT_DETECTED = 1;
static constexpr int32_t ERR_LINK_DOWN    = 2;

// ===================================================================
// Freshness timeout for health check
// ===================================================================
static constexpr uint32_t FRESHNESS_MS = 5000;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;
static ModuleRuntime* s_rt = nullptr;

// ===================================================================
// Config keys (NVS, namespace "agsteer")
// ===================================================================
namespace eth_keys {
    static constexpr const char* IP      = "mod_eth_ip";
    static constexpr const char* GATEWAY = "mod_eth_gw";
    static constexpr const char* SUBNET  = "mod_eth_mask";
    static constexpr const char* DNS     = "mod_eth_dns";
    static constexpr const char* MODE    = "mod_eth_mode";   // "dhcp" or "static"
}

// Module-local config cache
static struct {
    uint32_t ip      = 0;
    uint32_t gateway = 0;
    uint32_t subnet  = 0;
    uint32_t dns     = 0;
    char     mode[8] = "dhcp";
    bool     dirty   = false;
} s_cfg;

// ===================================================================
// Dependencies (none — ETH is the primary transport)
// ===================================================================
static const ModuleId s_deps[] = { ModuleId::COUNT };  // COUNT is sentinel

// ===================================================================
// IP formatting helper
// ===================================================================
static void formatU32Ip(uint32_t ip, char* buf, size_t len) {
    std::snprintf(buf, len, "%u.%u.%u.%u",
        (unsigned)((ip >> 24) & 0xFF),
        (unsigned)((ip >> 16) & 0xFF),
        (unsigned)((ip >> 8) & 0xFF),
        (unsigned)(ip & 0xFF));
}

/// Parse dotted-decimal IP string "a.b.c.d" → big-endian u32.
/// Returns 0 on parse failure.
static uint32_t parseDottedIp(const char* str) {
    if (!str) return 0;
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

// ===================================================================
// NVS helpers
// ===================================================================
#if defined(ARDUINO_ARCH_ESP32)
static bool ethNvsLoad(void) {
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &h) != ESP_OK) return false;

    uint32_t u32 = 0;
    if (nvs_get_u32(h, eth_keys::IP, &u32) == ESP_OK)      s_cfg.ip = u32;
    if (nvs_get_u32(h, eth_keys::GATEWAY, &u32) == ESP_OK) s_cfg.gateway = u32;
    if (nvs_get_u32(h, eth_keys::SUBNET, &u32) == ESP_OK)  s_cfg.subnet = u32;
    if (nvs_get_u32(h, eth_keys::DNS, &u32) == ESP_OK)     s_cfg.dns = u32;
    size_t len = sizeof(s_cfg.mode);
    if (nvs_get_str(h, eth_keys::MODE, s_cfg.mode, &len) != ESP_OK) {
        std::strncpy(s_cfg.mode, "dhcp", sizeof(s_cfg.mode));
    }
    s_cfg.mode[sizeof(s_cfg.mode) - 1] = '\0';
    s_cfg.dirty = false;
    nvs_close(h);
    return true;
}

static bool ethNvsSave(void) {
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READWRITE, &h) != ESP_OK) return false;

    bool ok = true;
    auto chk = [&](esp_err_t err, const char* k) {
        if (err != ESP_OK) { ok = false; LOGW(TAG, "NVS set '%s' failed", k); }
    };
    chk(nvs_set_u32(h, eth_keys::IP,      s_cfg.ip),      eth_keys::IP);
    chk(nvs_set_u32(h, eth_keys::GATEWAY, s_cfg.gateway), eth_keys::GATEWAY);
    chk(nvs_set_u32(h, eth_keys::SUBNET,  s_cfg.subnet),  eth_keys::SUBNET);
    chk(nvs_set_u32(h, eth_keys::DNS,     s_cfg.dns),     eth_keys::DNS);
    chk(nvs_set_str(h, eth_keys::MODE,    s_cfg.mode),    eth_keys::MODE);

    esp_err_t commit_err = nvs_commit(h);
    if (commit_err != ESP_OK) {
        ok = false;
        LOGW(TAG, "NVS commit failed");
    }
    nvs_close(h);
    s_cfg.dirty = false;
    return ok && (commit_err == ESP_OK);
}
#else
static bool ethNvsLoad(void) { return false; }
static bool ethNvsSave(void) { return false; }
#endif

// ===================================================================
// Lifecycle
// ===================================================================
static bool eth_is_enabled(void) {
    return true;
}

static void eth_activate(void) {
    s_rt = moduleSysGet(ModuleId::ETH);
    if (!s_rt) {
        LOGE(TAG, "moduleSysGet(ETH) returned nullptr");
        return;
    }

    // Load config from NVS before init
    ethNvsLoad();

    // If static mode configured, push IP settings to HAL before init
    if (std::strcmp(s_cfg.mode, "static") == 0 && s_cfg.ip != 0) {
        hal_net_set_static_config(s_cfg.ip, s_cfg.gateway, s_cfg.subnet);
    }

    hal_net_init();

    s_state.detected = hal_net_detected();
    s_state.error_code = s_state.detected ? 0 : ERR_NOT_DETECTED;
    s_state.last_update_ms = hal_millis();

    if (s_state.detected) {
        LOGI(TAG, "activated — chip detected, mode=%s", s_cfg.mode);
    } else {
        LOGW(TAG, "activated — chip NOT detected (error=%ld)", (long)s_state.error_code);
    }

    if (s_rt) s_rt->state = s_state;
}

static void eth_deactivate(void) {
    LOGI(TAG, "deactivated");
    s_state.detected = false;
    s_state.quality_ok = false;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
}

static bool eth_is_healthy(uint32_t now_ms) {
    if (!s_state.detected) {
        s_state.error_code = ERR_NOT_DETECTED;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

    if (!hal_net_is_connected()) {
        s_state.quality_ok = false;
        s_state.error_code = ERR_LINK_DOWN;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

    if ((now_ms - s_state.last_update_ms) > FRESHNESS_MS) {
        s_state.quality_ok = false;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

    s_state.quality_ok = true;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
    return true;
}

// ===================================================================
// Pipeline
// ===================================================================
static ModuleResult eth_input(uint32_t now_ms) {
    if (hal_net_is_connected()) {
        s_state.last_update_ms = now_ms;
        s_state.quality_ok = true;
        s_state.error_code = 0;
        if (s_rt) s_rt->state = s_state;
    }
    return MOD_OK;
}

static ModuleResult eth_process(uint32_t /*now_ms*/) {
    return MOD_OK;
}

static ModuleResult eth_output(uint32_t now_ms) {
    if (hal_net_is_connected()) {
        s_state.last_update_ms = now_ms;
        s_state.quality_ok = true;
        s_state.error_code = 0;
        if (s_rt) s_rt->state = s_state;
    }
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================
static bool eth_cfg_get(const char* key, char* buf, size_t len) {
    if (!key || !buf || len == 0) return false;

    if (std::strcmp(key, "mode") == 0) {
        std::strncpy(buf, s_cfg.mode, len);
        buf[len - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "ip") == 0) {
        formatU32Ip(s_cfg.ip, buf, len);
        return true;
    }
    if (std::strcmp(key, "gateway") == 0 || std::strcmp(key, "gw") == 0) {
        formatU32Ip(s_cfg.gateway, buf, len);
        return true;
    }
    if (std::strcmp(key, "subnet") == 0 || std::strcmp(key, "mask") == 0) {
        formatU32Ip(s_cfg.subnet, buf, len);
        return true;
    }
    if (std::strcmp(key, "dns") == 0) {
        formatU32Ip(s_cfg.dns, buf, len);
        return true;
    }
    return false;
}

static bool eth_cfg_set(const char* key, const char* val) {
    if (!key || !val) return false;

    if (std::strcmp(key, "mode") == 0) {
        if (std::strcmp(val, "dhcp") != 0 && std::strcmp(val, "static") != 0) {
            return false;  // invalid mode
        }
        std::strncpy(s_cfg.mode, val, sizeof(s_cfg.mode) - 1);
        s_cfg.mode[sizeof(s_cfg.mode) - 1] = '\0';
    } else if (std::strcmp(key, "ip") == 0) {
        s_cfg.ip = parseDottedIp(val);
        if (s_cfg.ip == 0) return false;
    } else if (std::strcmp(key, "gateway") == 0 || std::strcmp(key, "gw") == 0) {
        s_cfg.gateway = parseDottedIp(val);
    } else if (std::strcmp(key, "subnet") == 0 || std::strcmp(key, "mask") == 0) {
        s_cfg.subnet = parseDottedIp(val);
    } else if (std::strcmp(key, "dns") == 0) {
        s_cfg.dns = parseDottedIp(val);
    } else {
        return false;
    }

    s_cfg.dirty = true;
    return true;
}

static bool eth_cfg_apply(void) {
    // If static mode, push cached IP config to HAL and restart
    if (std::strcmp(s_cfg.mode, "static") == 0 && s_cfg.ip != 0) {
        hal_net_set_static_config(s_cfg.ip, s_cfg.gateway, s_cfg.subnet);
    }
    LOGI(TAG, "cfg_apply: restarting ETH (mode=%s)", s_cfg.mode);
    hal_net_restart();
    s_state.detected = hal_net_detected();
    s_state.last_update_ms = hal_millis();
    if (s_rt) s_rt->state = s_state;
    return true;
}

static bool eth_cfg_save(void) {
    return ethNvsSave();
}

static bool eth_cfg_load(void) {
    return ethNvsLoad();
}

static bool eth_cfg_show(void) {
    char ip_buf[20], gw_buf[20], sn_buf[20], dns_buf[20], live_ip[20], live_gw[20], live_sn[20];

    // Configured values
    formatU32Ip(s_cfg.ip, ip_buf, sizeof(ip_buf));
    formatU32Ip(s_cfg.gateway, gw_buf, sizeof(gw_buf));
    formatU32Ip(s_cfg.subnet, sn_buf, sizeof(sn_buf));
    formatU32Ip(s_cfg.dns, dns_buf, sizeof(dns_buf));

    s_cli_out->printf("  Mode:    %s\n", s_cfg.mode);
    if (std::strcmp(s_cfg.mode, "static") == 0) {
        s_cli_out->printf("  IP:      %s\n", ip_buf);
        s_cli_out->printf("  Gateway: %s\n", gw_buf);
        s_cli_out->printf("  Mask:    %s\n", sn_buf);
        s_cli_out->printf("  DNS:     %s\n", dns_buf);
    }

    // Live values (from ETH driver)
    if (hal_net_is_connected()) {
        formatU32Ip(hal_net_get_ip(), live_ip, sizeof(live_ip));
        formatU32Ip(hal_net_get_gateway(), live_gw, sizeof(live_gw));
        formatU32Ip(hal_net_get_subnet(), live_sn, sizeof(live_sn));
        s_cli_out->printf("  Live IP:   %s\n", live_ip);
        s_cli_out->printf("  Live GW:   %s\n", live_gw);
        s_cli_out->printf("  Live Mask: %s\n", live_sn);

        uint8_t speed = hal_net_link_speed();
        bool duplex = hal_net_full_duplex();
        s_cli_out->printf("  Link:     %u Mbps %s\n",
            (unsigned)speed, duplex ? "full-duplex" : "half-duplex");

        uint8_t mac[6];
        hal_net_get_mac(mac);
        s_cli_out->printf("  MAC:      %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        s_cli_out->printf("  Status:   %s\n", hal_net_link_up() ? "link up, no IP" : "DISCONNECTED");
    }

    if (s_cfg.dirty) {
        s_cli_out->println("  (unsaved changes — use 'save' to persist)");
    }

    return true;
}

// ===================================================================
// Debug — comprehensive ETH diagnostics
// ===================================================================
static bool eth_debug(void) {
    s_cli_out->println("=== ETH Diagnostics ===");

    // 1. Hardware detection
    s_cli_out->printf("  Chip detected:  %s\n", hal_net_detected() ? "YES" : "NO");

    // 2. Link state
    s_cli_out->printf("  Link:           %s\n", hal_net_link_up() ? "UP" : "DOWN");
    s_cli_out->printf("  Connected (IP): %s\n", hal_net_is_connected() ? "YES" : "NO");

    if (hal_net_is_connected()) {
        char buf[20];
        // 3. Live IP info
        formatU32Ip(hal_net_get_ip(), buf, sizeof(buf));
        s_cli_out->printf("  IP Address:    %s\n", buf);
        formatU32Ip(hal_net_get_gateway(), buf, sizeof(buf));
        s_cli_out->printf("  Gateway:       %s\n", buf);
        formatU32Ip(hal_net_get_subnet(), buf, sizeof(buf));
        s_cli_out->printf("  Subnet Mask:   %s\n", buf);
        formatU32Ip(hal_net_get_dns(), buf, sizeof(buf));
        s_cli_out->printf("  DNS:           %s\n", buf);

        // 4. Link details
        uint8_t speed = hal_net_link_speed();
        bool duplex = hal_net_full_duplex();
        s_cli_out->printf("  Speed:         %u Mbps %s\n",
            (unsigned)speed, duplex ? "full-duplex" : "half-duplex");

        // 5. MAC
        uint8_t mac[6];
        hal_net_get_mac(mac);
        s_cli_out->printf("  MAC:           %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // 6. Config vs Live comparison
    s_cli_out->println("--- Config vs Live ---");
    char cfg_ip[20], cfg_gw[20], cfg_sn[20];
    formatU32Ip(s_cfg.ip, cfg_ip, sizeof(cfg_ip));
    formatU32Ip(s_cfg.gateway, cfg_gw, sizeof(cfg_gw));
    formatU32Ip(s_cfg.subnet, cfg_sn, sizeof(cfg_sn));
    s_cli_out->printf("  Mode:  %s  (dirty=%s)\n", s_cfg.mode, s_cfg.dirty ? "YES" : "no");
    if (std::strcmp(s_cfg.mode, "static") == 0) {
        s_cli_out->printf("  Cfg IP:    %s\n", cfg_ip);
        s_cli_out->printf("  Cfg GW:    %s\n", cfg_gw);
        s_cli_out->printf("  Cfg Mask:  %s\n", cfg_sn);

        if (hal_net_is_connected()) {
            char live_ip[20], live_gw[20], live_sn[20];
            formatU32Ip(hal_net_get_ip(), live_ip, sizeof(live_ip));
            formatU32Ip(hal_net_get_gateway(), live_gw, sizeof(live_gw));
            formatU32Ip(hal_net_get_subnet(), live_sn, sizeof(live_sn));
            bool ip_match   = (s_cfg.ip == hal_net_get_ip());
            bool gw_match   = (s_cfg.gateway == hal_net_get_gateway());
            bool sn_match   = (s_cfg.subnet == hal_net_get_subnet());
            s_cli_out->printf("  Live IP:    %s  %s\n", live_ip, ip_match ? "OK" : "MISMATCH");
            s_cli_out->printf("  Live GW:    %s  %s\n", live_gw, gw_match ? "OK" : "MISMATCH");
            s_cli_out->printf("  Live Mask:  %s  %s\n", live_sn, sn_match ? "OK" : "MISMATCH");
        }
    }

    // 7. Module state
    s_cli_out->println("--- Module State ---");
    s_cli_out->printf("  Detected:    %s\n", s_state.detected ? "YES" : "NO");
    s_cli_out->printf("  Quality:     %s\n", s_state.quality_ok ? "OK" : "BAD");
    s_cli_out->printf("  Error:       %ld\n", (long)s_state.error_code);
    s_cli_out->printf("  LastUpdate:  %lu ms ago\n",
        (unsigned long)(hal_millis() - s_state.last_update_ms));

    s_cli_out->println("=== Done ===");
    return true;
}

// ===================================================================
// Ops table
// ===================================================================
const ModuleOps2 mod_eth_ops = {
    .name      = "ETH",
    .id        = ModuleId::ETH,

    .is_enabled  = eth_is_enabled,
    .activate    = eth_activate,
    .deactivate  = eth_deactivate,
    .is_healthy  = eth_is_healthy,

    .input   = eth_input,
    .process = eth_process,
    .output  = eth_output,

    .cfg_get   = eth_cfg_get,
    .cfg_set   = eth_cfg_set,
    .cfg_apply = eth_cfg_apply,
    .cfg_save  = eth_cfg_save,
    .cfg_load  = eth_cfg_load,
    .cfg_show  = eth_cfg_show,

    .debug = eth_debug,

    .deps = s_deps,
};
