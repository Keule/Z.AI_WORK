/**
 * @file config_pid.cpp
 * @brief PID-Konfigurations-Kategorie — ADR-006.
 *
 * Verwaltet PID-Regler-Parameter: Kp, Ki, Kd.
 * Validierung: positive Floats, sinnvolle Bereiche.
 *   - Kp: 0.0 – 10.0
 *   - Ki: 0.0 – 5.0
 *   - Kd: 0.0 – 1.0
 */

#include "config_pid.h"
#include "config_framework.h"
#include "runtime_config.h"
#include "mod_steer.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CFG
#include "esp_log.h"
#include "log_ext.h"

#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* TAG = "CFG-PID";

// ===================================================================
// Kategorie-Operationen
// ===================================================================

static bool config_pid_validate(void) {
    RuntimeConfig& cfg = softConfigGet();

    if (cfg.pid_kp < 0.0f || cfg.pid_kp > 10.0f) {
        LOGE(TAG, "Kp ausserhalb des Bereichs: %.3f (0-10)", cfg.pid_kp);
        return false;
    }
    if (cfg.pid_ki < 0.0f || cfg.pid_ki > 5.0f) {
        LOGE(TAG, "Ki ausserhalb des Bereichs: %.3f (0-5)", cfg.pid_ki);
        return false;
    }
    if (cfg.pid_kd < 0.0f || cfg.pid_kd > 1.0f) {
        LOGE(TAG, "Kd ausserhalb des Bereichs: %.3f (0-1)", cfg.pid_kd);
        return false;
    }

    return true;
}

static bool config_pid_apply(void) {
    RuntimeConfig& cfg = softConfigGet();
    mod_steer_set_pid_gains(cfg.pid_kp, cfg.pid_ki, cfg.pid_kd);
    LOGI(TAG, "PID angewendet: Kp=%.3f Ki=%.3f Kd=%.3f",
         cfg.pid_kp, cfg.pid_ki, cfg.pid_kd);
    return true;
}

static bool config_pid_load(void) {
    LOGI(TAG, "PID-Konfiguration aus NVS geladen (zentral)");
    return true;
}

static bool config_pid_save(void) {
    LOGI(TAG, "PID-Konfiguration in NVS gespeichert (zentral)");
    return true;
}

static void config_pid_show(ConfigStream output) {
    auto* out = static_cast<Stream*>(output);
    if (!out) return;
    RuntimeConfig& cfg = softConfigGet();

    out->println("=== PID Regler ===");
    out->printf("  Kp: %.3f  (Bereich: 0-10)\n", cfg.pid_kp);
    out->printf("  Ki: %.3f  (Bereich: 0-5)\n",  cfg.pid_ki);
    out->printf("  Kd: %.3f  (Bereich: 0-1)\n",  cfg.pid_kd);
}

static bool config_pid_set(const char* key, const char* value) {
    if (!key || !value) return false;

    RuntimeConfig& cfg = softConfigGet();
    float fval = static_cast<float>(std::atof(value));

    if (std::strcmp(key, "kp") == 0) {
        if (fval < 0.0f || fval > 10.0f) {
            LOGE(TAG, "Kp ausserhalb des Bereichs: %.3f (0-10)", fval);
            return false;
        }
        cfg.pid_kp = fval;
        return true;
    }

    if (std::strcmp(key, "ki") == 0) {
        if (fval < 0.0f || fval > 5.0f) {
            LOGE(TAG, "Ki ausserhalb des Bereichs: %.3f (0-5)", fval);
            return false;
        }
        cfg.pid_ki = fval;
        return true;
    }

    if (std::strcmp(key, "kd") == 0) {
        if (fval < 0.0f || fval > 1.0f) {
            LOGE(TAG, "Kd ausserhalb des Bereichs: %.3f (0-1)", fval);
            return false;
        }
        cfg.pid_kd = fval;
        return true;
    }

    LOGW(TAG, "Unbekannter Key: %s", key);
    return false;
}

static bool config_pid_get(const char* key, char* buf, size_t buf_size) {
    if (!key || !buf || buf_size == 0) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "kp") == 0) {
        std::snprintf(buf, buf_size, "%.3f", cfg.pid_kp);
        return true;
    }
    if (std::strcmp(key, "ki") == 0) {
        std::snprintf(buf, buf_size, "%.3f", cfg.pid_ki);
        return true;
    }
    if (std::strcmp(key, "kd") == 0) {
        std::snprintf(buf, buf_size, "%.3f", cfg.pid_kd);
        return true;
    }

    return false;
}

// ===================================================================
// Kategorie-Deskriptor
// ===================================================================

static const ConfigCategoryOps s_pid_ops = {
    CONFIG_CAT_PID,
    "PID",            // Name (deutsch)
    "pid",            // Name (englisch)
    config_pid_validate,
    config_pid_apply,
    config_pid_load,
    config_pid_save,
    config_pid_show,
    config_pid_set,
    config_pid_get
};

void configPidInit(void) {
    configFrameworkRegisterCategory(&s_pid_ops);
}
