/**
 * @file config_framework.cpp
 * @brief Basis-Konfig-Framework — ADR-006 (TASK-037).
 *
 * Zentrale Registry fuer alle Konfigurations-Kategorien.
 * Bietet einheitliche Schnittstelle fuer Validierung, Persistierung
 * und Anzeige aller Konfigurationswerte.
 *
 * Aenderungen sind nur im PAUSED-Modus zulaessig (isEditable).
 */

#include "config_framework.h"
#include "op_mode.h"
#include "nvs_config.h"
#include "runtime_config.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CFG
#include "esp_log.h"
#include "log_ext.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <strings.h>

static const char* TAG = "CFGFW";

// ===================================================================
// Category Registry
// ===================================================================

/// Statische Registry fuer alle Konfigurations-Kategorien.
static const ConfigCategoryOps* s_categories[CONFIG_MAX_CATEGORIES] = {};
static size_t s_category_count = 0;

// ===================================================================
// Public API
// ===================================================================

void configFrameworkInit(void) {
    s_category_count = 0;
    for (size_t i = 0; i < CONFIG_MAX_CATEGORIES; i++) {
        s_categories[i] = nullptr;
    }
    LOGI(TAG, "initialisiert (max %d Kategorien)", CONFIG_MAX_CATEGORIES);
}

bool configFrameworkRegisterCategory(const ConfigCategoryOps* ops) {
    if (!ops || !ops->name) {
        LOGE(TAG, "ungueltige Kategorie-Registration (nullptr)");
        return false;
    }

    if (s_category_count >= CONFIG_MAX_CATEGORIES) {
        LOGE(TAG, "Registry voll — Kategorie '%s' nicht registriert", ops->name);
        return false;
    }

    // Duplikat-Check nach Kategorie-ID
    for (size_t i = 0; i < s_category_count; i++) {
        if (s_categories[i]->category == ops->category) {
            LOGW(TAG, "Kategorie %d bereits registriert als '%s'",
                 (int)ops->category, s_categories[i]->name);
            return false;
        }
    }

    s_categories[s_category_count++] = ops;
    LOGI(TAG, "Kategorie registriert: %s (id=%d)", ops->name, (int)ops->category);
    return true;
}

const ConfigCategoryOps* configFrameworkFindCategory(ConfigCategory cat) {
    for (size_t i = 0; i < s_category_count; i++) {
        if (s_categories[i]->category == cat) {
            return s_categories[i];
        }
    }
    return nullptr;
}

const ConfigCategoryOps* configFrameworkFindCategoryByName(const char* name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < s_category_count; i++) {
        if (s_categories[i]->name && strcasecmp(name, s_categories[i]->name) == 0) {
            return s_categories[i];
        }
        if (s_categories[i]->name_en && strcasecmp(name, s_categories[i]->name_en) == 0) {
            return s_categories[i];
        }
    }
    return nullptr;
}

size_t configFrameworkCategoryCount(void) {
    return s_category_count;
}

bool configFrameworkIsEditable(void) {
    return opModeIsPaused();
}

bool configFrameworkValidateAll(void) {
    bool all_valid = true;
    for (size_t i = 0; i < s_category_count; i++) {
        const ConfigCategoryOps* ops = s_categories[i];
        if (ops->validate) {
            if (!ops->validate()) {
                LOGE(TAG, "Validierung fehlgeschlagen: %s", ops->name);
                all_valid = false;
            }
        }
    }
    return all_valid;
}

bool configFrameworkApplyAll(void) {
    bool all_ok = true;
    for (size_t i = 0; i < s_category_count; i++) {
        const ConfigCategoryOps* ops = s_categories[i];
        if (ops->apply) {
            if (!ops->apply()) {
                LOGW(TAG, "Apply fehlgeschlagen: %s", ops->name);
                all_ok = false;
            }
        }
    }
    return all_ok;
}

bool configFrameworkSaveAll(void) {
    // Erst validieren — nur gueltige Konfiguration speichern
    if (!configFrameworkValidateAll()) {
        LOGE(TAG, "SaveAll abgebrochen — Validierung fehlgeschlagen");
        return false;
    }

    // Alle Kategorien speichern (synct statische Werte nach RuntimeConfig)
    bool all_ok = true;
    for (size_t i = 0; i < s_category_count; i++) {
        const ConfigCategoryOps* ops = s_categories[i];
        if (ops->save) {
            if (!ops->save()) {
                LOGE(TAG, "Save fehlgeschlagen: %s", ops->name);
                all_ok = false;
            }
        }
    }

    // RuntimeConfig in NVS schreiben (einheitlicher Speicherpunkt)
    if (all_ok) {
        RuntimeConfig& cfg = softConfigGet();
        if (!nvsConfigSave(cfg)) {
            LOGE(TAG, "nvsConfigSave fehlgeschlagen");
            all_ok = false;
        }
    }

    return all_ok;
}

bool configFrameworkLoadAll(void) {
    bool all_ok = true;
    for (size_t i = 0; i < s_category_count; i++) {
        const ConfigCategoryOps* ops = s_categories[i];
        if (ops->load) {
            if (!ops->load()) {
                LOGW(TAG, "Load fehlgeschlagen: %s", ops->name);
                all_ok = false;
            }
        }
    }
    return all_ok;
}

void configFrameworkFactoryReset(void) {
    LOGW(TAG, "Werkseinstellungen — NVS wird geloescht");
    nvsConfigFactoryReset();

    // Defaults neu laden
    RuntimeConfig& cfg = softConfigGet();
    softConfigLoadDefaults(cfg);

    LOGI(TAG, "Werkseinstellungen geladen — Neustart empfohlen");
}

void configFrameworkShowCategory(ConfigCategory cat, ConfigStream output) {
    const ConfigCategoryOps* ops = configFrameworkFindCategory(cat);
    if (ops && ops->show) {
        ops->show(output);
    } else {
        auto* out = static_cast<Stream*>(output);
        if (out) {
            out->printf("Kategorie %d nicht gefunden oder keine Show-Funktion\n", (int)cat);
        }
    }
}

void configFrameworkShowAll(ConfigStream output) {
    for (size_t i = 0; i < s_category_count; i++) {
        const ConfigCategoryOps* ops = s_categories[i];
        if (ops->show) {
            ops->show(output);
        }
        auto* out = static_cast<Stream*>(output);
        if (out) out->println();
    }
}

bool configFrameworkSet(ConfigCategory cat, const char* key, const char* value) {
    if (!configFrameworkIsEditable()) {
        LOGW(TAG, "Set abgelehnt — nicht im PAUSED Modus");
        return false;
    }
    if (!key || !value) {
        LOGE(TAG, "Set: key oder value ist nullptr");
        return false;
    }

    const ConfigCategoryOps* ops = configFrameworkFindCategory(cat);
    if (!ops) {
        LOGE(TAG, "Set: Kategorie %d nicht gefunden", (int)cat);
        return false;
    }

    if (!ops->set) {
        LOGE(TAG, "Set: Kategorie '%s' hat keine Set-Funktion", ops->name);
        return false;
    }

    if (!ops->set(key, value)) {
        LOGW(TAG, "Set: Kategorie '%s', Key '%s' — fehlgeschlagen", ops->name, key);
        return false;
    }

    LOGI(TAG, "Set: %s.%s = %s", ops->name, key, value);
    return true;
}

bool configFrameworkGet(ConfigCategory cat, const char* key, char* buf, size_t buf_size) {
    if (!key || !buf || buf_size == 0) return false;

    const ConfigCategoryOps* ops = configFrameworkFindCategory(cat);
    if (!ops || !ops->get) return false;

    return ops->get(key, buf, buf_size);
}

void configFrameworkPrintStatus(ConfigStream output) {
    auto* out = static_cast<Stream*>(output);
    if (!out) return;

    out->printf("=== Config Framework: %d/%d Kategorien ===\n",
                (int)s_category_count, CONFIG_MAX_CATEGORIES);

    const char* mode = configFrameworkIsEditable() ? "EDITABLE" : "READ-ONLY";
    out->printf("  Modus: %s (%s)\n", mode, opModeToString(opModeGet()));

    for (size_t i = 0; i < s_category_count; i++) {
        const ConfigCategoryOps* ops = s_categories[i];
        const char* validate_str = "?";
        if (ops->validate) {
            validate_str = ops->validate() ? "OK" : "FEHLER";
        } else {
            validate_str = "-";
        }
        out->printf("  [%d] %-12s %s\n", (int)ops->category, ops->name, validate_str);
    }
}
