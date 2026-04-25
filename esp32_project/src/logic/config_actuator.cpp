/**
 * @file config_actuator.cpp
 * @brief Aktuator-Konfigurations-Kategorie — ADR-006.
 *
 * Verwaltet Aktuator-Typ: 0=SPI, 1=Cytron, 2=IBT2.
 */

#include "config_actuator.h"
#include "config_framework.h"
#include "runtime_config.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CFG
#include "esp_log.h"
#include "log_ext.h"

#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

static const char* TAG = "CFG-ACT";

// ===================================================================
// Hilfsfunktionen
// ===================================================================

static const char* actuatorTypeStr(uint8_t type) {
    switch (type) {
        case 0:  return "SPI";
        case 1:  return "Cytron";
        case 2:  return "IBT2";
        default: return "UNBEKANNT";
    }
}

static bool parseActuatorType(const char* text, uint8_t* out) {
    if (!text || !out) return false;

    // Numerisch
    if (std::strlen(text) == 1 && text[0] >= '0' && text[0] <= '2') {
        *out = static_cast<uint8_t>(text[0] - '0');
        return true;
    }

    // Textuell (case-insensitiv)
    if (strcasecmp(text, "spi") == 0)   { *out = 0; return true; }
    if (strcasecmp(text, "cytron") == 0) { *out = 1; return true; }
    if (strcasecmp(text, "ibt2") == 0)   { *out = 2; return true; }

    return false;
}

// ===================================================================
// Kategorie-Operationen
// ===================================================================

static bool config_actuator_validate(void) {
    RuntimeConfig& cfg = softConfigGet();
    if (cfg.actuator_type > 2) {
        LOGE(TAG, "Ungueltiger Aktuator-Typ: %u (0=SPI, 1=Cytron, 2=IBT2)",
             (unsigned)cfg.actuator_type);
        return false;
    }
    return true;
}

static bool config_actuator_apply(void) {
    // Aktuator-Typ wird beim naechsten Neustart/Reinit wirksam
    RuntimeConfig& cfg = softConfigGet();
    LOGI(TAG, "Aktuator-Typ: %s (%u)", actuatorTypeStr(cfg.actuator_type),
         (unsigned)cfg.actuator_type);
    return true;
}

static bool config_actuator_load(void) {
    LOGI(TAG, "Aktuator-Konfiguration aus NVS geladen (zentral)");
    return true;
}

static bool config_actuator_save(void) {
    LOGI(TAG, "Aktuator-Konfiguration in NVS gespeichert (zentral)");
    return true;
}

static void config_actuator_show(ConfigStream output) {
    auto* out = static_cast<Stream*>(output);
    if (!out) return;
    RuntimeConfig& cfg = softConfigGet();

    out->println("=== Aktuator ===");
    out->printf("  Typ: %s (%u)\n", actuatorTypeStr(cfg.actuator_type),
                (unsigned)cfg.actuator_type);
    out->println("  Optionen: 0=SPI, 1=Cytron, 2=IBT2");
}

static bool config_actuator_set(const char* key, const char* value) {
    if (!key || !value) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "type") == 0 || std::strcmp(key, "actuator_type") == 0) {
        uint8_t type = 0;
        if (!parseActuatorType(value, &type)) {
            LOGE(TAG, "Ungueltiger Aktuator-Typ: %s (0=SPI, 1=Cytron, 2=IBT2)", value);
            return false;
        }
        cfg.actuator_type = type;
        return true;
    }

    LOGW(TAG, "Unbekannter Key: %s", key);
    return false;
}

static bool config_actuator_get(const char* key, char* buf, size_t buf_size) {
    if (!key || !buf || buf_size == 0) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "type") == 0 || std::strcmp(key, "actuator_type") == 0) {
        std::snprintf(buf, buf_size, "%u", (unsigned)cfg.actuator_type);
        return true;
    }
    if (std::strcmp(key, "name") == 0) {
        std::strncpy(buf, actuatorTypeStr(cfg.actuator_type), buf_size - 1);
        buf[buf_size - 1] = '\0';
        return true;
    }

    return false;
}

// ===================================================================
// Kategorie-Deskriptor
// ===================================================================

static const ConfigCategoryOps s_actuator_ops = {
    CONFIG_CAT_ACTUATOR,
    "Aktuator",       // Name (deutsch)
    "actuator",       // Name (englisch)
    config_actuator_validate,
    config_actuator_apply,
    config_actuator_load,
    config_actuator_save,
    config_actuator_show,
    config_actuator_set,
    config_actuator_get
};

void configActuatorInit(void) {
    configFrameworkRegisterCategory(&s_actuator_ops);
}
