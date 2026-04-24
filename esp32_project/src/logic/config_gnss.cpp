/**
 * @file config_gnss.cpp
 * @brief GNSS-Konfigurations-Kategorie — ADR-006 (TASK-041, TASK-043).
 *
 * GNSS Parameter: Baudrate, UART-A/B Rollen und Baudraten.
 * Vollstaendige Implementierung mit NVS-Load/Save, Validierung,
 * UART-Reinitialisierung bei Apply und Warnung bei doppelten Rollen.
 */

#include "config_gnss.h"
#include "config_framework.h"
#include "runtime_config.h"
#include "um980_uart_setup.h"
#include "nvs_config.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CFG
#include "esp_log.h"
#include "log_ext.h"

#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* TAG = "CFG-GNSS";

// ===================================================================
// Statische UART-Rollen/Baud (separat von RuntimeConfig, da
// UART-Rollen nicht im RuntimeConfig struct sind)
// ===================================================================

static uint8_t  s_uart_a_role = UART_ROLE_DISABLED;
static uint8_t  s_uart_b_role = UART_ROLE_DISABLED;
static uint32_t s_uart_a_baud = 921600;
static uint32_t s_uart_b_baud = 921600;

// ===================================================================
// Hilfsfunktionen
// ===================================================================

/// Zulaessige Baudraten-Liste.
static const uint32_t kValidBauds[] = {
    4800, 9600, 19200, 38400, 57600,
    115200, 230400, 460800, 921600
};
static constexpr size_t kValidBaudCount = sizeof(kValidBauds) / sizeof(kValidBauds[0]);

bool configGnssIsValidBaud(uint32_t baud) {
    for (size_t i = 0; i < kValidBaudCount; i++) {
        if (kValidBauds[i] == baud) return true;
    }
    return false;
}

const char* configGnssUartRoleName(uint8_t role) {
    switch (role) {
        case UART_ROLE_NMEA:     return "NMEA";
        case UART_ROLE_RTCM:     return "RTCM";
        case UART_ROLE_DIAG:     return "DIAG";
        case UART_ROLE_DISABLED: return "DISABLED";
        default:                 return "UNKNOWN";
    }
}

// ===================================================================
// Kategorie-Operationen
// ===================================================================

static bool config_gnss_validate(void) {
    RuntimeConfig& cfg = softConfigGet();
    const uint32_t baud = cfg.gnss_baud;

    // GNSS-Baudrate validieren
    if (!configGnssIsValidBaud(baud)) {
        LOGE(TAG, "Ungueltige GNSS-Baudrate: %lu", (unsigned long)baud);
        return false;
    }

    // UART-A Baudrate validieren
    if (!configGnssIsValidBaud(s_uart_a_baud)) {
        LOGE(TAG, "Ungueltige UART-A Baudrate: %lu", (unsigned long)s_uart_a_baud);
        return false;
    }

    // UART-B Baudrate validieren
    if (!configGnssIsValidBaud(s_uart_b_baud)) {
        LOGE(TAG, "Ungueltige UART-B Baudrate: %lu", (unsigned long)s_uart_b_baud);
        return false;
    }

    // Warnung bei gleichen Rollen (kein Fehler)
    if (s_uart_a_role != UART_ROLE_DISABLED &&
        s_uart_a_role == s_uart_b_role) {
        LOGW(TAG, "WARNUNG: Beide UARTs haben dieselbe Rolle (%s)",
             configGnssUartRoleName(s_uart_a_role));
    }

    // Rollen-Bereich pruefen
    if (s_uart_a_role > UART_ROLE_DISABLED || s_uart_b_role > UART_ROLE_DISABLED) {
        LOGE(TAG, "Ungueltige UART-Rolle (A=%u, B=%u)",
             (unsigned)s_uart_a_role, (unsigned)s_uart_b_role);
        return false;
    }

    return true;
}

static bool config_gnss_apply(void) {
    RuntimeConfig& cfg = softConfigGet();

    // UM980 Setup mit aktuellen Werten konfigurieren
    um980SetupSetBaud(0, s_uart_a_baud);
    um980SetupSetBaud(1, s_uart_b_baud);
    um980SetupSetRole(0, s_uart_a_role);
    um980SetupSetRole(1, s_uart_b_role);

    // UARTs reinitialisieren
    const bool ok = um980SetupApply();
    LOGI(TAG, "GNSS Konfiguration angewendet (baud=%lu, A=%s/%lu, B=%s/%lu) -> %s",
         (unsigned long)cfg.gnss_baud,
         configGnssUartRoleName(s_uart_a_role), (unsigned long)s_uart_a_baud,
         configGnssUartRoleName(s_uart_b_role), (unsigned long)s_uart_b_baud,
         ok ? "OK" : "FAIL");
    return ok;
}

static bool config_gnss_load(void) {
    RuntimeConfig& cfg = softConfigGet();
    // Per-Port UART Baud und Rollen aus RuntimeConfig (bereits aus NVS geladen) uebernehmen
    s_uart_a_baud = cfg.gnss_uart_a_baud;
    s_uart_b_baud = cfg.gnss_uart_b_baud;
    s_uart_a_role = cfg.gnss_uart_a_role;
    s_uart_b_role = cfg.gnss_uart_b_role;
    LOGI(TAG, "GNSS-Konfiguration geladen (A: %s/%lu, B: %s/%lu)",
         configGnssUartRoleName(s_uart_a_role), (unsigned long)s_uart_a_baud,
         configGnssUartRoleName(s_uart_b_role), (unsigned long)s_uart_b_baud);
    return true;
}

static bool config_gnss_save(void) {
    // Per-Port UART Werte zurueck in RuntimeConfig schreiben, damit
    // nvsConfigSave() sie mit speichert
    RuntimeConfig& cfg = softConfigGet();
    cfg.gnss_uart_a_baud = s_uart_a_baud;
    cfg.gnss_uart_b_baud = s_uart_b_baud;
    cfg.gnss_uart_a_role = s_uart_a_role;
    cfg.gnss_uart_b_role = s_uart_b_role;
    LOGI(TAG, "GNSS-Konfiguration in RuntimeConfig gespeichert (A: %s/%lu, B: %s/%lu)",
         configGnssUartRoleName(s_uart_a_role), (unsigned long)s_uart_a_baud,
         configGnssUartRoleName(s_uart_b_role), (unsigned long)s_uart_b_baud);
    return true;
}

void configGnssShow(ConfigStream output) {
    auto* out = static_cast<Stream*>(output);
    if (!out) return;
    RuntimeConfig& cfg = softConfigGet();

    out->println("=== GNSS ===");
    out->printf("  Baudrate:   %lu\n", (unsigned long)cfg.gnss_baud);
    out->printf("  UART A:     %s @ %lu baud\n",
                configGnssUartRoleName(s_uart_a_role),
                (unsigned long)s_uart_a_baud);
    out->printf("  UART B:     %s @ %lu baud\n",
                configGnssUartRoleName(s_uart_b_role),
                (unsigned long)s_uart_b_baud);

    if (s_uart_a_role != UART_ROLE_DISABLED &&
        s_uart_a_role == s_uart_b_role) {
        out->println("  WARNUNG: Beide UARTs haben dieselbe Rolle!");
    }
}

static void config_gnss_show(ConfigStream output) {
    configGnssShow(output);
}

/// Rolle-String in UartRole umwandeln.
static bool parseUartRole(const char* text, uint8_t* out_role) {
    if (!text || !out_role) return false;
    if (std::strcmp(text, "nmea") == 0)     { *out_role = UART_ROLE_NMEA;     return true; }
    if (std::strcmp(text, "rtcm") == 0)     { *out_role = UART_ROLE_RTCM;     return true; }
    if (std::strcmp(text, "diag") == 0)     { *out_role = UART_ROLE_DIAG;     return true; }
    if (std::strcmp(text, "disabled") == 0 ||
        std::strcmp(text, "off") == 0)      { *out_role = UART_ROLE_DISABLED;  return true; }
    // Numerisch
    int val = std::atoi(text);
    if (val >= 0 && val <= 3) {
        *out_role = static_cast<uint8_t>(val);
        return true;
    }
    return false;
}

static bool config_gnss_set(const char* key, const char* value) {
    if (!key || !value) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "baud") == 0 || std::strcmp(key, "baudrate") == 0) {
        uint32_t baud = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
        if (!configGnssIsValidBaud(baud)) {
            LOGE(TAG, "Ungueltige Baudrate: %lu", (unsigned long)baud);
            return false;
        }
        cfg.gnss_baud = baud;
        return true;
    }

    if (std::strcmp(key, "uart_a_role") == 0 || std::strcmp(key, "role_a") == 0) {
        return parseUartRole(value, &s_uart_a_role);
    }

    if (std::strcmp(key, "uart_b_role") == 0 || std::strcmp(key, "role_b") == 0) {
        return parseUartRole(value, &s_uart_b_role);
    }

    if (std::strcmp(key, "uart_a_baud") == 0 || std::strcmp(key, "baud_a") == 0) {
        uint32_t baud = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
        if (!configGnssIsValidBaud(baud)) return false;
        s_uart_a_baud = baud;
        return true;
    }

    if (std::strcmp(key, "uart_b_baud") == 0 || std::strcmp(key, "baud_b") == 0) {
        uint32_t baud = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
        if (!configGnssIsValidBaud(baud)) return false;
        s_uart_b_baud = baud;
        return true;
    }

    LOGW(TAG, "Unbekannter Key: %s", key);
    return false;
}

static bool config_gnss_get(const char* key, char* buf, size_t buf_size) {
    if (!key || !buf || buf_size == 0) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "baud") == 0 || std::strcmp(key, "baudrate") == 0) {
        std::snprintf(buf, buf_size, "%lu", (unsigned long)cfg.gnss_baud);
        return true;
    }
    if (std::strcmp(key, "uart_a_role") == 0 || std::strcmp(key, "role_a") == 0) {
        std::snprintf(buf, buf_size, "%s", configGnssUartRoleName(s_uart_a_role));
        return true;
    }
    if (std::strcmp(key, "uart_b_role") == 0 || std::strcmp(key, "role_b") == 0) {
        std::snprintf(buf, buf_size, "%s", configGnssUartRoleName(s_uart_b_role));
        return true;
    }
    if (std::strcmp(key, "uart_a_baud") == 0 || std::strcmp(key, "baud_a") == 0) {
        std::snprintf(buf, buf_size, "%lu", (unsigned long)s_uart_a_baud);
        return true;
    }
    if (std::strcmp(key, "uart_b_baud") == 0 || std::strcmp(key, "baud_b") == 0) {
        std::snprintf(buf, buf_size, "%lu", (unsigned long)s_uart_b_baud);
        return true;
    }

    return false;
}

// ===================================================================
// Kategorie-Deskriptor
// ===================================================================

static const ConfigCategoryOps s_gnss_ops = {
    CONFIG_CAT_GNSS,
    "GNSS",          // Name (deutsch)
    "gnss",          // Name (englisch)
    config_gnss_validate,
    config_gnss_apply,
    config_gnss_load,
    config_gnss_save,
    config_gnss_show,
    config_gnss_set,
    config_gnss_get
};

void configGnssInit(void) {
    configFrameworkRegisterCategory(&s_gnss_ops);
}
