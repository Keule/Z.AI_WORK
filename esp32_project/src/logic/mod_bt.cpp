/**
 * @file mod_bt.cpp
 * @brief Transport module: Bluetooth SPP (ModuleId::BT).
 *
 * Stub implementation. Starts a BluetoothSerial SPP server named
 * "AgSteer-Boot" for diagnostic serial access over Bluetooth.
 * No PGN data is routed over BT in the current firmware.
 */

#include "mod_bt.h"
#include "hal/hal.h"
#include "nvs_config.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MOD
#include "esp_log.h"
#include "log_ext.h"

#include <cstring>
#include <cstdio>

#if defined(ARDUINO_ARCH_ESP32)
#if defined(__has_include) && __has_include(<BluetoothSerial.h>)
#include <BluetoothSerial.h>
#define BT_SPP_AVAILABLE 1
#else
#define BT_SPP_AVAILABLE 0
#endif
#endif

#if defined(ARDUINO_ARCH_ESP32)
#include <nvs.h>
#endif

static const char* const TAG = "MOD_BT";

// ===================================================================
// Error codes
// ===================================================================
static constexpr int32_t ERR_INIT_FAILED = 1;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;
static ModuleRuntime* s_rt = nullptr;

// ===================================================================
// BluetoothSerial instance (only if available)
// ===================================================================
#if BT_SPP_AVAILABLE
static BluetoothSerial s_bt_serial;
#endif

// ===================================================================
// Config keys (NVS, namespace "agsteer")
// ===================================================================
namespace bt_keys {
    static constexpr const char* NAME = "mod_bt_name";
}

// Module-local config cache
static struct {
    char name[33] = "AgSteer-Boot";
    bool dirty    = false;
} s_cfg;

// ===================================================================
// Dependencies (none)
// ===================================================================
static const ModuleId s_deps[] = { ModuleId::COUNT };

// ===================================================================
// NVS helpers
// ===================================================================
#if defined(ARDUINO_ARCH_ESP32)
static bool btNvsLoad(void) {
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &h) != ESP_OK) return false;

    size_t len = sizeof(s_cfg.name);
    if (nvs_get_str(h, bt_keys::NAME, s_cfg.name, &len) == ESP_OK) {
        s_cfg.name[sizeof(s_cfg.name) - 1] = '\0';
    }
    s_cfg.dirty = false;
    nvs_close(h);
    return true;
}

static bool btNvsSave(void) {
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READWRITE, &h) != ESP_OK) return false;

    bool ok = true;
    auto chk = [&](esp_err_t err, const char* k) {
        if (err != ESP_OK) { ok = false; LOGW(TAG, "NVS set '%s' failed", k); }
    };
    chk(nvs_set_str(h, bt_keys::NAME, s_cfg.name), bt_keys::NAME);

    esp_err_t commit_err = nvs_commit(h);
    if (commit_err != ESP_OK) { ok = false; LOGW(TAG, "NVS commit failed"); }
    nvs_close(h);
    s_cfg.dirty = false;
    return ok && (commit_err == ESP_OK);
}
#else
static bool btNvsLoad(void) { return false; }
static bool btNvsSave(void) { return false; }
#endif

// ===================================================================
// Lifecycle
// ===================================================================
static bool bt_is_enabled(void) {
    return true;
}

static void bt_activate(void) {
    s_rt = moduleSysGet(ModuleId::BT);
    if (!s_rt) {
        LOGE(TAG, "moduleSysGet(BT) returned nullptr");
        return;
    }

    btNvsLoad();

#if BT_SPP_AVAILABLE
    if (s_bt_serial.begin(s_cfg.name)) {
        s_state.detected = true;
        s_state.error_code = 0;
        LOGI(TAG, "activated — SPP \"%s\"", s_cfg.name);
    } else {
        s_state.detected = false;
        s_state.error_code = ERR_INIT_FAILED;
        LOGE(TAG, "activated — BluetoothSerial.begin() failed");
    }
#else
    LOGW(TAG, "activated — BluetoothSerial not available (stub)");
    s_state.detected = false;
    s_state.error_code = ERR_INIT_FAILED;
#endif

    s_state.last_update_ms = hal_millis();
    if (s_rt) s_rt->state = s_state;
}

static void bt_deactivate(void) {
#if BT_SPP_AVAILABLE
    s_bt_serial.end();
    LOGI(TAG, "deactivated — SPP stopped");
#else
    LOGI(TAG, "deactivated (stub)");
#endif
    s_state.detected = false;
    s_state.quality_ok = false;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
}

static bool bt_is_healthy(uint32_t /*now_ms*/) {
    if (!s_state.detected) {
        s_state.error_code = ERR_INIT_FAILED;
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
// Pipeline (all No-Op)
// ===================================================================
static ModuleResult bt_input(uint32_t /*now_ms*/) {
    return MOD_OK;
}

static ModuleResult bt_process(uint32_t /*now_ms*/) {
    return MOD_OK;
}

static ModuleResult bt_output(uint32_t /*now_ms*/) {
    return MOD_OK;
}

// ===================================================================
// Configuration
// ===================================================================
static bool bt_cfg_get(const char* key, char* buf, size_t len) {
    if (!key || !buf || len == 0) return false;

#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t h = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &h) != ESP_OK) return false;

    char full_key[64];
    std::snprintf(full_key, sizeof(full_key), "mod_bt_%s", key);

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

static bool bt_cfg_set(const char* key, const char* val) {
    if (!key || !val) return false;

    if (std::strcmp(key, "name") == 0) {
        std::strncpy(s_cfg.name, val, sizeof(s_cfg.name) - 1);
        s_cfg.name[sizeof(s_cfg.name) - 1] = '\0';
    } else {
        return false;
    }

    s_cfg.dirty = true;
    LOGI(TAG, "cfg_set: %s = %s", key, val);
    return true;
}

static bool bt_cfg_apply(void) {
    // Requires deactivate/reactivate to take effect
    LOGI(TAG, "cfg_apply: name=%s (requires re-activate)", s_cfg.name);
    return true;
}

static bool bt_cfg_save(void) {
    return btNvsSave();
}

static bool bt_cfg_load(void) {
    return btNvsLoad();
}

static bool bt_cfg_show(void) {
    LOGI(TAG, "config: name=%s", s_cfg.name);
#if BT_SPP_AVAILABLE
    LOGI(TAG, "  SPP available: yes");
#else
    LOGI(TAG, "  SPP available: no (stub)");
#endif
    LOGI(TAG, "  detected=%d quality_ok=%d error=%ld",
         s_state.detected, s_state.quality_ok, (long)s_state.error_code);
    return true;
}

// ===================================================================
// Debug
// ===================================================================
static bool bt_debug(void) {
    LOGI(TAG, "=== BT Debug ===");
    bt_cfg_show();
    return true;
}

// ===================================================================
static const CfgKeyDef s_bt_keys[] = {
    {"name",     "Bluetooth device name"},
    {nullptr, nullptr}
};

static const CfgKeyDef* bt_cfg_keys(void) { return s_bt_keys; }

// Ops table
// ===================================================================
const ModuleOps2 mod_bt_ops = {
    .name      = "BT",
    .id        = ModuleId::BT,

    .is_enabled  = bt_is_enabled,
    .activate    = bt_activate,
    .deactivate  = bt_deactivate,
    .is_healthy  = bt_is_healthy,

    .input   = bt_input,
    .process = bt_process,
    .output  = bt_output,

    .cfg_keys   = bt_cfg_keys,
    .cfg_get   = bt_cfg_get,
    .cfg_set   = bt_cfg_set,
    .cfg_apply = bt_cfg_apply,
    .cfg_save  = bt_cfg_save,
    .cfg_load  = bt_cfg_load,
    .cfg_show  = bt_cfg_show,

    .debug = bt_debug,

    .deps = s_deps,
};
