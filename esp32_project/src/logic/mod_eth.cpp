/**
 * @file mod_eth.cpp
 * @brief Transport module: W5500 Ethernet (ModuleId::ETH).
 *
 * Wraps hal_net_* functions into the ModuleOps2 interface.
 * Migrated from net.cpp Ethernet initialisation.
 */

#include "mod_eth.h"
#include "hal/hal.h"
#include "nvs_config.h"

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
    static constexpr const char* IP     = "mod_eth_ip";
    static constexpr const char* GATEWAY = "mod_eth_gateway";
    static constexpr const char* SUBNET  = "mod_eth_subnet";
    static constexpr const char* MODE    = "mod_eth_mode";   // "dhcp" or "static"
}

// Module-local config cache
static struct {
    uint32_t ip      = 0;
    uint32_t gateway = 0;
    uint32_t subnet  = 0;
    char     mode[16] = "dhcp";
    bool     dirty   = false;
} s_cfg;

// ===================================================================
// Dependencies (none — ETH is the primary transport)
// ===================================================================
static const ModuleId s_deps[] = { ModuleId::COUNT };  // nullptr-terminated (COUNT is sentinel)

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
    // W5500 Ethernet is always compiled in (primary transport).
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

    hal_net_init();

    s_state.detected = hal_net_detected();
    s_state.error_code = s_state.detected ? 0 : ERR_NOT_DETECTED;
    s_state.last_update_ms = hal_millis();

    if (s_state.detected) {
        LOGI(TAG, "activated — W5500 detected, mode=%s", s_cfg.mode);
    } else {
        LOGW(TAG, "activated — W5500 NOT detected (error=%ld)", (long)s_state.error_code);
    }

    // Copy state into runtime
    if (s_rt) s_rt->state = s_state;
}

static void eth_deactivate(void) {
    // ETH is always available — nothing to release.
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

    // Freshness check
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
    // NOTE: PGN RX is handled by mod_network.input(), NOT here.
    // ETH only monitors link status.
    if (hal_net_is_connected()) {
        s_state.last_update_ms = now_ms;
        s_state.quality_ok = true;
        s_state.error_code = 0;
        if (s_rt) s_rt->state = s_state;
    }
    return MOD_OK;
}

static ModuleResult eth_process(uint32_t /*now_ms*/) {
    // No-Op: PGN processing is handled by mod_network.
    return MOD_OK;
}

static ModuleResult eth_output(uint32_t now_ms) {
    // NOTE: PGN TX is handled by mod_network.output(), NOT here.
    // ETH only monitors link status.
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

#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &h) != ESP_OK) return false;

    // Build full key: "mod_eth_<key>"
    char full_key[64];
    std::snprintf(full_key, sizeof(full_key), "mod_eth_%s", key);

    // Try as string first, then as u32
    size_t str_len = len;
    if (nvs_get_str(h, full_key, buf, &str_len) == ESP_OK) {
        buf[len - 1] = '\0';
        nvs_close(h);
        return true;
    }

    // Try as u32 (format as decimal string)
    uint32_t u32 = 0;
    if (nvs_get_u32(h, full_key, &u32) == ESP_OK) {
        std::snprintf(buf, len, "%lu", (unsigned long)u32);
        nvs_close(h);
        return true;
    }

    nvs_close(h);
#else
    (void)len;
#endif
    return false;
}

static bool eth_cfg_set(const char* key, const char* val) {
    if (!key || !val) return false;

    // Update local cache
    if (std::strcmp(key, "ip") == 0) {
        s_cfg.ip = static_cast<uint32_t>(std::strtoul(val, nullptr, 10));
    } else if (std::strcmp(key, "gateway") == 0) {
        s_cfg.gateway = static_cast<uint32_t>(std::strtoul(val, nullptr, 10));
    } else if (std::strcmp(key, "subnet") == 0) {
        s_cfg.subnet = static_cast<uint32_t>(std::strtoul(val, nullptr, 10));
    } else if (std::strcmp(key, "mode") == 0) {
        std::strncpy(s_cfg.mode, val, sizeof(s_cfg.mode) - 1);
        s_cfg.mode[sizeof(s_cfg.mode) - 1] = '\0';
    } else {
        return false;
    }

    s_cfg.dirty = true;
    LOGI(TAG, "cfg_set: %s = %s", key, val);
    return true;
}

static bool eth_cfg_apply(void) {
    // If static mode, push cached IP config to HAL
    if (std::strcmp(s_cfg.mode, "static") == 0 && s_cfg.ip != 0) {
        hal_net_set_static_config(s_cfg.ip, s_cfg.gateway, s_cfg.subnet);
        LOGI(TAG, "cfg_apply: static mode, restarting Ethernet");
        hal_net_restart();
    }
    return true;
}

static bool eth_cfg_save(void) {
    return ethNvsSave();
}

static bool eth_cfg_load(void) {
    return ethNvsLoad();
}

static bool eth_cfg_show(void) {
    uint32_t ip = s_cfg.ip;
    uint32_t gw = s_cfg.gateway;
    uint32_t sn = s_cfg.subnet;
    LOGI(TAG, "config: mode=%s ip=%lu.%lu.%lu.%lu gw=%lu.%lu.%lu.%lu subnet=%lu.%lu.%lu.%lu",
         s_cfg.mode,
         (unsigned long)((ip >> 24) & 0xFF), (unsigned long)((ip >> 16) & 0xFF),
         (unsigned long)((ip >> 8) & 0xFF),  (unsigned long)(ip & 0xFF),
         (unsigned long)((gw >> 24) & 0xFF), (unsigned long)((gw >> 16) & 0xFF),
         (unsigned long)((gw >> 8) & 0xFF),  (unsigned long)(gw & 0xFF),
         (unsigned long)((sn >> 24) & 0xFF), (unsigned long)((sn >> 16) & 0xFF),
         (unsigned long)((sn >> 8) & 0xFF),  (unsigned long)(sn & 0xFF));
    LOGI(TAG, "  detected=%d quality_ok=%d error=%ld",
         s_state.detected, s_state.quality_ok, (long)s_state.error_code);
    return true;
}

// ===================================================================
// Debug
// ===================================================================
static bool eth_debug(void) {
    LOGI(TAG, "=== ETH Debug ===");
    eth_cfg_show();
    LOGI(TAG, "  hal_net_detected()  = %d", hal_net_detected());
    LOGI(TAG, "  hal_net_is_connected() = %d", hal_net_is_connected());
    LOGI(TAG, "  hal_net_link_up()    = %d", hal_net_link_up());
    uint32_t ip = hal_net_get_ip();
    LOGI(TAG, "  hal_net_get_ip()     = %lu.%lu.%lu.%lu",
         (unsigned long)((ip >> 24) & 0xFF), (unsigned long)((ip >> 16) & 0xFF),
         (unsigned long)((ip >> 8) & 0xFF),  (unsigned long)(ip & 0xFF));
    return true;
}

// ===================================================================
// Config key definitions
// ===================================================================
static const CfgKeyDef s_eth_keys[] = {
    {"mode",    "\"dhcp\" or \"static\""},
    {"ip",      "Static IPv4 (e.g. 192.168.1.70)"},
    {"gateway", "Static gateway (e.g. 192.168.1.1)"},
    {"subnet",  "Subnet mask (e.g. 255.255.255.0)"},
    {"dns",     "DNS server (e.g. 8.8.8.8)"},
    {nullptr, nullptr}  // sentinel
};

static const CfgKeyDef* eth_cfg_keys(void) { return s_eth_keys; }

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

    .cfg_keys   = eth_cfg_keys,
    .cfg_get   = eth_cfg_get,
    .cfg_set   = eth_cfg_set,
    .cfg_apply = eth_cfg_apply,
    .cfg_save  = eth_cfg_save,
    .cfg_load  = eth_cfg_load,
    .cfg_show  = eth_cfg_show,

    .debug = eth_debug,

    .deps = s_deps,
};
