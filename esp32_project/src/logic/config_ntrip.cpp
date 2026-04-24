/**
 * @file config_ntrip.cpp
 * @brief NTRIP-Konfigurations-Kategorie — ADR-006 (TASK-042).
 *
 * NTRIP Client Parameter: Host, Port, Mountpoint, User, Password, Reconnect.
 * Vollstaendige Implementierung mit NVS-Load/Save, Validierung,
 * Passwort-Maskierung und NTRIP-Reconnect bei Apply.
 */

#include "config_ntrip.h"
#include "config_framework.h"
#include "runtime_config.h"
#include "ntrip.h"
#include "nvs_config.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CFG
#include "esp_log.h"
#include "log_ext.h"

#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* TAG = "CFG-NTRIP";

// ===================================================================
// Hilfsfunktionen
// ===================================================================

/// Maskiertes Passwort fuer Ausgabe. Immer "****" wenn gesetzt, leer wenn nicht.
static const char* kMaskedPassword = "****";

// ===================================================================
// Kategorie-Operationen
// ===================================================================

static bool config_ntrip_validate(void) {
    RuntimeConfig& cfg = softConfigGet();

    // Port-Bereich pruefen (1-65535, 0 = deaktiviert)
    if (cfg.ntrip_port > 65535) {
        LOGE(TAG, "Port ausserhalb des Bereichs: %u", (unsigned)cfg.ntrip_port);
        return false;
    }

    // Wenn host gesetzt ist, muss auch mountpoint gesetzt sein
    if (cfg.ntrip_host[0] != '\0' && cfg.ntrip_mountpoint[0] == '\0') {
        LOGE(TAG, "Host ohne Mountpoint angegeben");
        return false;
    }

    // Mountpoint ohne Host ist nicht erlaubt
    if (cfg.ntrip_mountpoint[0] != '\0' && cfg.ntrip_host[0] == '\0') {
        LOGE(TAG, "Mountpoint ohne Host angegeben");
        return false;
    }

    // Reconnect-Bereich: 1000-60000 ms
    if (cfg.ntrip_reconnect_ms < 1000 || cfg.ntrip_reconnect_ms > 60000) {
        LOGE(TAG, "Reconnect ausserhalb des Bereichs: %lu ms (erlaubt: 1000-60000)",
             (unsigned long)cfg.ntrip_reconnect_ms);
        return false;
    }

    // user/password: keine Validierung (koennen leer sein fuer offene Caster)

    return true;
}

static bool config_ntrip_apply(void) {
    RuntimeConfig& cfg = softConfigGet();

    // NTRIP-Konfiguration an Client uebergeben
    // ntripSetConfig kopiert die Strings in den NTRIP-Status (thread-safe)
    ntripSetConfig(cfg.ntrip_host, cfg.ntrip_port, cfg.ntrip_mountpoint,
                   cfg.ntrip_user, cfg.ntrip_password);

    // Reconnect-Delay setzen
    ntripSetReconnectDelay(cfg.ntrip_reconnect_ms);

    LOGI(TAG, "NTRIP Konfiguration angewendet (host=%s, port=%u, reconnect=%lu ms)",
         cfg.ntrip_host[0] ? cfg.ntrip_host : "(disabled)",
         (unsigned)cfg.ntrip_port,
         (unsigned long)cfg.ntrip_reconnect_ms);
    return true;
}

static bool config_ntrip_load(void) {
    // NVS Load wird zentral in nvsConfigLoad() erledigt.
    // Hier nur Logging.
    LOGI(TAG, "NTRIP-Konfiguration aus NVS geladen");
    return true;
}

static bool config_ntrip_save(void) {
    // NVS Save wird zentral in nvsConfigSave() erledigt.
    // Hier nur Logging.
    LOGI(TAG, "NTRIP-Konfiguration in NVS gespeichert");
    return true;
}

void configNtripShow(ConfigStream output) {
    auto* out = static_cast<Stream*>(output);
    if (!out) return;
    RuntimeConfig& cfg = softConfigGet();

    out->println("=== NTRIP ===");
    out->printf("  Host:       %s\n", cfg.ntrip_host[0] ? cfg.ntrip_host : "(disabled)");
    out->printf("  Port:       %u\n", (unsigned)cfg.ntrip_port);
    out->printf("  Mountpoint: %s\n", cfg.ntrip_mountpoint[0] ? cfg.ntrip_mountpoint : "(none)");
    out->printf("  User:       %s\n", cfg.ntrip_user[0] ? cfg.ntrip_user : "(none)");
    // Passwort IMMER maskieren — CRITICAL SECURITY
    out->printf("  Password:   %s\n", cfg.ntrip_password[0] ? kMaskedPassword : "(empty)");
    out->printf("  Reconnect:  %lu ms\n", (unsigned long)cfg.ntrip_reconnect_ms);
}

static void config_ntrip_show(ConfigStream output) {
    configNtripShow(output);
}

static bool config_ntrip_set(const char* key, const char* value) {
    if (!key || !value) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "host") == 0) {
        std::strncpy(cfg.ntrip_host, value, sizeof(cfg.ntrip_host) - 1);
        cfg.ntrip_host[sizeof(cfg.ntrip_host) - 1] = '\0';
        return true;
    }

    if (std::strcmp(key, "port") == 0) {
        int port = std::atoi(value);
        if (port < 0 || port > 65535) return false;
        cfg.ntrip_port = static_cast<uint16_t>(port);
        return true;
    }

    if (std::strcmp(key, "mountpoint") == 0 || std::strcmp(key, "mount") == 0) {
        std::strncpy(cfg.ntrip_mountpoint, value, sizeof(cfg.ntrip_mountpoint) - 1);
        cfg.ntrip_mountpoint[sizeof(cfg.ntrip_mountpoint) - 1] = '\0';
        return true;
    }

    if (std::strcmp(key, "user") == 0) {
        std::strncpy(cfg.ntrip_user, value, sizeof(cfg.ntrip_user) - 1);
        cfg.ntrip_user[sizeof(cfg.ntrip_user) - 1] = '\0';
        return true;
    }

    if (std::strcmp(key, "pass") == 0 || std::strcmp(key, "password") == 0) {
        std::strncpy(cfg.ntrip_password, value, sizeof(cfg.ntrip_password) - 1);
        cfg.ntrip_password[sizeof(cfg.ntrip_password) - 1] = '\0';
        // Passwort wurde aktualisiert — LOG ohne Klartext!
        LOGI(TAG, "NTRIP Passwort aktualisiert");
        return true;
    }

    if (std::strcmp(key, "reconnect") == 0) {
        uint32_t ms = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
        if (ms < 1000 || ms > 60000) return false;
        cfg.ntrip_reconnect_ms = ms;
        return true;
    }

    LOGW(TAG, "Unbekannter Key: %s", key);
    return false;
}

static bool config_ntrip_get(const char* key, char* buf, size_t buf_size) {
    if (!key || !buf || buf_size == 0) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "host") == 0) {
        std::strncpy(buf, cfg.ntrip_host, buf_size - 1);
        buf[buf_size - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "port") == 0) {
        std::snprintf(buf, buf_size, "%u", (unsigned)cfg.ntrip_port);
        return true;
    }
    if (std::strcmp(key, "mountpoint") == 0 || std::strcmp(key, "mount") == 0) {
        std::strncpy(buf, cfg.ntrip_mountpoint, buf_size - 1);
        buf[buf_size - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "user") == 0) {
        std::strncpy(buf, cfg.ntrip_user, buf_size - 1);
        buf[buf_size - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "pass") == 0 || std::strcmp(key, "password") == 0) {
        // Passwort NIE im Klartext zurueckgeben — CRITICAL SECURITY
        std::strncpy(buf, cfg.ntrip_password[0] ? kMaskedPassword : "", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return true;
    }
    if (std::strcmp(key, "reconnect") == 0) {
        std::snprintf(buf, buf_size, "%lu", (unsigned long)cfg.ntrip_reconnect_ms);
        return true;
    }

    return false;
}

// ===================================================================
// Kategorie-Deskriptor
// ===================================================================

static const ConfigCategoryOps s_ntrip_ops = {
    CONFIG_CAT_NTRIP,
    "NTRIP",         // Name (deutsch)
    "ntrip",         // Name (englisch)
    config_ntrip_validate,
    config_ntrip_apply,
    config_ntrip_load,
    config_ntrip_save,
    config_ntrip_show,
    config_ntrip_set,
    config_ntrip_get
};

void configNtripInit(void) {
    configFrameworkRegisterCategory(&s_ntrip_ops);
}
