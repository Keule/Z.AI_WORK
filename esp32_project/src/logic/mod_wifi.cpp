/**
 * @file mod_wifi.cpp
 * @brief Transport module: WiFi AP/STA (ModuleId::WIFI).
 *
 * Stub implementation. WiFi provides a fallback AP for configuration
 * and diagnostic access. PGN data is not routed over WiFi in the
 * current firmware — all PGN traffic goes through Ethernet.
 */

#include "mod_wifi.h"
#include "hal/hal.h"
#include "nvs_config.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MOD
#include "esp_log.h"
#include "log_ext.h"

#include <cstring>
#include <cstdio>

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#endif

#if defined(ARDUINO_ARCH_ESP32)
#include <nvs.h>
#endif

static const char* const TAG = "MOD_WIFI";

// ===================================================================
// Error codes
// ===================================================================
static constexpr int32_t ERR_INIT_FAILED = 1;

// ===================================================================
// Freshness timeout
// ===================================================================
static constexpr uint32_t FRESHNESS_MS = 10000;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;
static ModuleRuntime* s_rt = nullptr;

// ===================================================================
// Config keys (NVS, namespace "agsteer")
// ===================================================================
namespace wifi_keys {
    static constexpr const char* SSID     = "mod_wifi_ssid";
    static constexpr const char* PASSWORD = "mod_wifi_password";
    static constexpr const char* MODE     = "mod_wifi_mode";  // "ap" or "sta"
}

// Module-local config cache
static struct {
    char ssid[33]     = "AgSteer-Boot";
    char password[64] = "";
    char mode[8]      = "ap";
    bool dirty        = false;
} s_cfg;

// ===================================================================
// Dependencies (none)
// ===================================================================
static const ModuleId s_deps[] = { ModuleId::COUNT };

// ===================================================================
// NVS helpers
// ===================================================================
#if defined(ARDUINO_ARCH_ESP32)
static bool wifiNvsLoad(void) {
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &h) != ESP_OK) return false;

    size_t len = sizeof(s_cfg.ssid);
    if (nvs_get_str(h, wifi_keys::SSID, s_cfg.ssid, &len) == ESP_OK) {
        s_cfg.ssid[sizeof(s_cfg.ssid) - 1] = '\0';
    }
    len = sizeof(s_cfg.password);
    if (nvs_get_str(h, wifi_keys::PASSWORD, s_cfg.password, &len) == ESP_OK) {
        s_cfg.password[sizeof(s_cfg.password) - 1] = '\0';
    }
    len = sizeof(s_cfg.mode);
    if (nvs_get_str(h, wifi_keys::MODE, s_cfg.mode, &len) == ESP_OK) {
        s_cfg.mode[sizeof(s_cfg.mode) - 1] = '\0';
    }
    s_cfg.dirty = false;
    nvs_close(h);
    return true;
}

static bool wifiNvsSave(void) {
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READWRITE, &h) != ESP_OK) return false;

    bool ok = true;
    auto chk = [&](esp_err_t err, const char* k) {
        if (err != ESP_OK) { ok = false; LOGW(TAG, "NVS set '%s' failed", k); }
    };
    chk(nvs_set_str(h, wifi_keys::SSID,     s_cfg.ssid),     wifi_keys::SSID);
    chk(nvs_set_str(h, wifi_keys::PASSWORD, s_cfg.password), wifi_keys::PASSWORD);
    chk(nvs_set_str(h, wifi_keys::MODE,     s_cfg.mode),     wifi_keys::MODE);

    esp_err_t commit_err = nvs_commit(h);
    if (commit_err != ESP_OK) { ok = false; LOGW(TAG, "NVS commit failed"); }
    nvs_close(h);
    s_cfg.dirty = false;
    return ok && (commit_err == ESP_OK);
}
#else
static bool wifiNvsLoad(void) { return false; }
static bool wifiNvsSave(void) { return false; }
#endif

// ===================================================================
// Lifecycle
// ===================================================================
static bool wifi_is_enabled(void) {
    return true;  // WiFi can always be compiled
}

static void wifi_activate(void) {
    s_rt = moduleSysGet(ModuleId::WIFI);
    if (!s_rt) {
        LOGE(TAG, "moduleSysGet(WIFI) returned nullptr");
        return;
    }

    wifiNvsLoad();

#if defined(ARDUINO_ARCH_ESP32)
    if (std::strcmp(s_cfg.mode, "sta") == 0) {
        WiFi.mode(WIFI_STA);
        if (s_cfg.ssid[0] != '\0') {
            WiFi.begin(s_cfg.ssid, s_cfg.password[0] ? s_cfg.password : nullptr);
            LOGI(TAG, "STA mode — connecting to \"%s\"", s_cfg.ssid);
        }
    } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(s_cfg.ssid, s_cfg.password[0] ? s_cfg.password : nullptr);
        LOGI(TAG, "AP mode — SSID=\"%s\"", s_cfg.ssid);
    }

    s_state.detected = true;
    s_state.error_code = 0;
#else
    LOGW(TAG, "WiFi not available on this platform (stub)");
    s_state.detected = false;
    s_state.error_code = ERR_INIT_FAILED;
#endif

    s_state.last_update_ms = hal_millis();
    if (s_rt) s_rt->state = s_state;
}

static void wifi_deactivate(void) {
#if defined(ARDUINO_ARCH_ESP32)
    WiFi.mode(WIFI_OFF);
    LOGI(TAG, "deactivated — WiFi OFF");
#else
    LOGI(TAG, "deactivated (stub)");
#endif
    s_state.detected = false;
    s_state.quality_ok = false;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
}

static bool wifi_is_healthy(uint32_t now_ms) {
    if (!s_state.detected) {
        s_state.error_code = ERR_INIT_FAILED;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

#if defined(ARDUINO_ARCH_ESP32)
    bool has_activity = false;
    if (WiFi.getMode() & WIFI_AP) {
        has_activity = (WiFi.softAPgetStationNum() > 0);
    } else if (WiFi.getMode() & WIFI_STA) {
        has_activity = (WiFi.status() == WL_CONNECTED);
    }

    if (!has_activity) {
        s_state.quality_ok = false;
        if ((now_ms - s_state.last_update_ms) > FRESHNESS_MS) {
            if (s_rt) s_rt->state = s_state;
            return false;
        }
        // Within grace period — still considered healthy (booting)
    } else {
        s_state.quality_ok = true;
        s_state.last_update_ms = now_ms;
    }
#else
    s_state.quality_ok = false;
#endif

    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
    return true;
}

// ===================================================================
// Pipeline (all No-Op — WiFi data not used for PGN in current impl)
// ===================================================================
static ModuleResult wifi_input(uint32_t /*now_ms*/) {
    return MOD_OK;
}

static ModuleResult wifi_process(uint32_t /*now_ms*/) {
    return MOD_OK;
}

static ModuleResult wifi_output(uint32_t /*now_ms*/) {
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================
static bool wifi_cfg_get(const char* key, char* buf, size_t len) {
    if (!key || !buf || len == 0) return false;

#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &h) != ESP_OK) return false;

    char full_key[64];
    std::snprintf(full_key, sizeof(full_key), "mod_wifi_%s", key);

    size_t str_len = len;
    if (nvs_get_str(h, full_key, buf, &str_len) == ESP_OK) {
        buf[len - 1] = '\0';
        nvs_close(h);
        return true;
    }

    nvs_close(h);
#else
    (void)len;
#endif
    return false;
}

static bool wifi_cfg_set(const char* key, const char* val) {
    if (!key || !val) return false;

    if (std::strcmp(key, "ssid") == 0) {
        std::strncpy(s_cfg.ssid, val, sizeof(s_cfg.ssid) - 1);
        s_cfg.ssid[sizeof(s_cfg.ssid) - 1] = '\0';
    } else if (std::strcmp(key, "password") == 0) {
        std::strncpy(s_cfg.password, val, sizeof(s_cfg.password) - 1);
        s_cfg.password[sizeof(s_cfg.password) - 1] = '\0';
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

static bool wifi_cfg_apply(void) {
    // Re-initialise WiFi with new config
    LOGI(TAG, "cfg_apply: mode=%s ssid=%s", s_cfg.mode, s_cfg.ssid);
    return true;
}

static bool wifi_cfg_save(void) {
    return wifiNvsSave();
}

static bool wifi_cfg_load(void) {
    return wifiNvsLoad();
}

static bool wifi_cfg_show(void) {
    LOGI(TAG, "config: mode=%s ssid=%s", s_cfg.mode, s_cfg.ssid);
#if defined(ARDUINO_ARCH_ESP32)
    if (WiFi.getMode() & WIFI_AP) {
        LOGI(TAG, "  AP active, stations=%d", WiFi.softAPgetStationNum());
        LOGI(TAG, "  IP=%s", WiFi.softAPIP().toString().c_str());
    } else if (WiFi.getMode() & WIFI_STA) {
        LOGI(TAG, "  STA status=%d", WiFi.status());
        if (WiFi.status() == WL_CONNECTED) {
            LOGI(TAG, "  IP=%s", WiFi.localIP().toString().c_str());
        }
    }
#endif
    LOGI(TAG, "  detected=%d quality_ok=%d error=%ld",
         s_state.detected, s_state.quality_ok, (long)s_state.error_code);
    return true;
}

// ===================================================================
// Debug
// ===================================================================
static bool wifi_debug(void) {
    LOGI(TAG, "=== WiFi Debug ===");
    wifi_cfg_show();
    return true;
}

// ===================================================================
// Ops table
// ===================================================================
const ModuleOps2 mod_wifi_ops = {
    .name      = "WIFI",
    .id        = ModuleId::WIFI,

    .is_enabled  = wifi_is_enabled,
    .activate    = wifi_activate,
    .deactivate  = wifi_deactivate,
    .is_healthy  = wifi_is_healthy,

    .input   = wifi_input,
    .process = wifi_process,
    .output  = wifi_output,

    .cfg_get   = wifi_cfg_get,
    .cfg_set   = wifi_cfg_set,
    .cfg_apply = wifi_cfg_apply,
    .cfg_save  = wifi_cfg_save,
    .cfg_load  = wifi_cfg_load,
    .cfg_show  = wifi_cfg_show,

    .debug = wifi_debug,

    .deps = s_deps,
};
