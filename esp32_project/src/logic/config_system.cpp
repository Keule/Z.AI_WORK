/**
 * @file config_system.cpp
 * @brief System-Konfigurations-Kategorie — ADR-006.
 *
 * Verwaltet: log_interval_ms, actuator_type.
 * Validierung: Intervall-Bereich (10-60000 ms), Aktuator-Typ (0-2).
 */

#include "config_system.h"
#include "config_framework.h"
#include "runtime_config.h"
#include "nvs_config.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CFG
#include "esp_log.h"
#include "log_ext.h"

#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* TAG = "CFG-SYS";

// ===================================================================
// Kategorie-Operationen
// ===================================================================

static bool config_system_validate(void) {
    RuntimeConfig& cfg = softConfigGet();

    // Log-Intervall: 10 ms bis 60 s
    if (cfg.log_interval_ms < 10 || cfg.log_interval_ms > 60000) {
        LOGE(TAG, "Ungueltiges Log-Intervall: %lu ms (10-60000)",
             (unsigned long)cfg.log_interval_ms);
        return false;
    }

    // Aktuator-Typ: 0=SPI, 1=Cytron, 2=IBT2
    if (cfg.actuator_type > 2) {
        LOGE(TAG, "Ungueltiger Aktuator-Typ: %u (0=SPI, 1=Cytron, 2=IBT2)",
             (unsigned)cfg.actuator_type);
        return false;
    }

    return true;
}

static bool config_system_apply(void) {
    // System-Konfiguration hat keine direkte Hardware-Auswirkung
    // Aktuator-Typ wird beim naechsten Neustart wirksam
    LOGI(TAG, "System-Konfiguration angewendet");
    return true;
}

static bool config_system_load(void) {
    LOGI(TAG, "System-Konfiguration aus NVS geladen (zentral)");
    return true;
}

static bool config_system_save(void) {
    LOGI(TAG, "System-Konfiguration in NVS gespeichert (zentral)");
    return true;
}

static const char* actuatorTypeName(uint8_t type) {
    switch (type) {
        case 0:  return "SPI";
        case 1:  return "Cytron";
        case 2:  return "IBT2";
        default: return "UNBEKANNT";
    }
}

static void config_system_show(ConfigStream output) {
    auto* out = static_cast<Stream*>(output);
    if (!out) return;
    RuntimeConfig& cfg = softConfigGet();

    out->println("=== System ===");
    out->printf("  Log-Intervall: %lu ms (%.1f Hz)\n",
                (unsigned long)cfg.log_interval_ms,
                cfg.log_interval_ms > 0 ? 1000.0f / static_cast<float>(cfg.log_interval_ms) : 0.0f);
    out->printf("  Log Aktiv:     %s\n", cfg.log_default_active ? "ja" : "nein");
    out->printf("  Aktuator:      %s (%u)\n",
                actuatorTypeName(cfg.actuator_type),
                (unsigned)cfg.actuator_type);
}

static bool config_system_set(const char* key, const char* value) {
    if (!key || !value) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "log_interval") == 0 || std::strcmp(key, "interval") == 0) {
        uint32_t interval = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
        if (interval < 10 || interval > 60000) {
            LOGE(TAG, "Log-Intervall ausserhalb des Bereichs: %lu", (unsigned long)interval);
            return false;
        }
        cfg.log_interval_ms = interval;
        return true;
    }

    if (std::strcmp(key, "log_active") == 0) {
        if (std::strcmp(value, "on") == 0 || std::strcmp(value, "1") == 0) {
            cfg.log_default_active = true;
        } else if (std::strcmp(value, "off") == 0 || std::strcmp(value, "0") == 0) {
            cfg.log_default_active = false;
        } else {
            return false;
        }
        return true;
    }

    if (std::strcmp(key, "actuator_type") == 0 || std::strcmp(key, "actuator") == 0) {
        uint8_t type = static_cast<uint8_t>(std::atoi(value));
        if (type > 2) {
            LOGE(TAG, "Aktuator-Typ ausserhalb des Bereichs: %u", (unsigned)type);
            return false;
        }
        cfg.actuator_type = type;
        return true;
    }

    LOGW(TAG, "Unbekannter Key: %s", key);
    return false;
}

static bool config_system_get(const char* key, char* buf, size_t buf_size) {
    if (!key || !buf || buf_size == 0) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "log_interval") == 0 || std::strcmp(key, "interval") == 0) {
        std::snprintf(buf, buf_size, "%lu", (unsigned long)cfg.log_interval_ms);
        return true;
    }

    if (std::strcmp(key, "log_active") == 0) {
        std::strncpy(buf, cfg.log_default_active ? "on" : "off", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return true;
    }

    if (std::strcmp(key, "actuator_type") == 0 || std::strcmp(key, "actuator") == 0) {
        std::snprintf(buf, buf_size, "%u", (unsigned)cfg.actuator_type);
        return true;
    }

    return false;
}

// ===================================================================
// Kategorie-Deskriptor
// ===================================================================

static const ConfigCategoryOps s_system_ops = {
    CONFIG_CAT_SYSTEM,
    "System",         // Name (deutsch)
    "system",         // Name (englisch)
    config_system_validate,
    config_system_apply,
    config_system_load,
    config_system_save,
    config_system_show,
    config_system_set,
    config_system_get
};

void configSystemInit(void) {
    configFrameworkRegisterCategory(&s_system_ops);
}
