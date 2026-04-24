/**
 * @file log_ext.cpp
 * @brief Runtime-Logging-Implementierung: Filter-Zustand und Serial-Kommandos.
 *
 * Serial-Kommandos:
 *   log <tag> <level>       Runtime-Level pro Tag setzen
 *   log all <level>         Alle Tags setzen
 *   log status              Aktuelle Konfiguration anzeigen
 *   filter <file:line>      Nur diese Zeile zeigen
 *   filter <file>           Ganze Datei zeigen
 *   filter off              Filter aufheben
 */

#include "log_ext.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ===================================================================
// Laufzeit-Filter-Zustand
// ===================================================================
uint16_t log_filter_line = 0;
char     log_filter_file[64] = "";

// ===================================================================
// Bekannte Tags (fuer "log all" Kommando)
// ADR-LOG-002: Liste muss vollstaendig sein und bei neuen Tags
// obligatorisch im selben Change-Set aktualisiert werden.
// ===================================================================
static const char* const kAllTags[] = {
    "MAIN", "NET", "CTL", "MOD", "HWS", "PGN",
    "WAS",  "IMU", "ACT", "SDL", "OTA", "HAL"
};
static constexpr size_t kTagCount = sizeof(kAllTags) / sizeof(kAllTags[0]);

// ===================================================================
// Intern: Level-String parsen
// ===================================================================
static int parseLevel(const char* s) {
    if (!s || !*s) return -1;
    if (std::strcmp(s, "none") == 0 || std::strcmp(s, "off") == 0)    return 0;
    if (std::strcmp(s, "error") == 0) return 1;
    if (std::strcmp(s, "warn") == 0)  return 2;
    if (std::strcmp(s, "info") == 0)  return 3;
    if (std::strcmp(s, "debug") == 0) return 4;
    if (std::strcmp(s, "verbose") == 0) return 5;
    return -1;
}

static const char* levelToStr(esp_log_level_t lvl) {
    switch (lvl) {
        case ESP_LOG_NONE:    return "NONE";
        case ESP_LOG_ERROR:   return "ERROR";
        case ESP_LOG_WARN:    return "WARN";
        case ESP_LOG_INFO:    return "INFO";
        case ESP_LOG_DEBUG:   return "DEBUG";
        case ESP_LOG_VERBOSE: return "VERBOSE";
        default:              return "???";
    }
}

// ===================================================================
// Forward declaration
// ===================================================================
static void logPrintStatusFn(void);

// ===================================================================
// Serial-Kommando verarbeiten
// ===================================================================
void logProcessSerialCmd(const char* cmd) {
    if (!cmd || !*cmd) return;

    // Fuehrende Whitespaces ueberspringen
    while (*cmd == ' ' || *cmd == '\t') cmd++;

    // --- "log ..." Kommandos ---
    if (std::strncmp(cmd, "log ", 4) == 0) {
        const char* arg = cmd + 4;
        while (*arg == ' ') arg++;

        // "log status"
        if (std::strcmp(arg, "status") == 0) {
            logPrintStatusFn();
            return;
        }

        // "log <tag|all> <level>"
        char tag[16] = "";
        char level_str[16] = "";
        if (std::sscanf(arg, "%15s %15s", tag, level_str) != 2) {
            ESP_LOGW("LOG", "usage: log <tag|all> <none|error|warn|info|debug|verbose>");
            return;
        }

        int level = parseLevel(level_str);
        if (level < 0) {
            ESP_LOGW("LOG", "unknown level: '%s'", level_str);
            return;
        }

        if (std::strcmp(tag, "all") == 0) {
            for (size_t i = 0; i < kTagCount; i++) {
                esp_log_level_set(kAllTags[i], (esp_log_level_t)level);
            }
            ESP_LOGI("LOG", "all tags -> %s", level_str);
        } else {
            esp_log_level_set(tag, (esp_log_level_t)level);
            ESP_LOGI("LOG", "%s -> %s", tag, level_str);
        }
        return;
    }

    // --- "filter ..." Kommandos ---
    if (std::strncmp(cmd, "filter ", 7) == 0) {
        const char* arg = cmd + 7;
        while (*arg == ' ') arg++;

        // "filter off"
        if (std::strcmp(arg, "off") == 0) {
            log_filter_line = 0;
            log_filter_file[0] = '\0';
            ESP_LOGI("LOG", "filter disabled");
            return;
        }

        // "filter <file>" oder "filter <file>:<line>"
        char file[48] = "";
        const char* colon = std::strchr(arg, ':');

        if (colon) {
            size_t flen = (size_t)(colon - arg);
            if (flen >= sizeof(file)) flen = sizeof(file) - 1;
            std::memcpy(file, arg, flen);
            file[flen] = '\0';

            int line = std::atoi(colon + 1);
            if (line <= 0) {
                // "filter net.cpp:" ohne Zeile -> ganze Datei
                log_filter_line = 0xFFFF;
            } else {
                log_filter_line = (uint16_t)line;
            }
        } else {
            // "filter net.cpp" -> ganze Datei
            std::strncpy(file, arg, sizeof(file) - 1);
            file[sizeof(file) - 1] = '\0';
            log_filter_line = 0xFFFF;
        }

        if (file[0]) {
            std::strncpy(log_filter_file, file, sizeof(log_filter_file) - 1);
            log_filter_file[sizeof(log_filter_file) - 1] = '\0';
            if (log_filter_line == 0xFFFF) {
                ESP_LOGI("LOG", "filter -> %s (all lines)", _log_basename(log_filter_file));
            } else {
                ESP_LOGI("LOG", "filter -> %s:%d", _log_basename(log_filter_file), (int)log_filter_line);
            }
        }
        return;
    }
}

// ===================================================================
// Status ausgeben (oeffentliche API)
// ===================================================================
void logPrintStatus(void) {
    logPrintStatusFn();
}

// ===================================================================
// Status ausgeben (interne Implementierung)
// ===================================================================
static void logPrintStatusFn(void) {
    ESP_LOGI("LOG", "=== Log Configuration ===");

    // Filter-Status
    if (log_filter_line == 0) {
        ESP_LOGI("LOG", "filter: OFF");
    } else if (log_filter_line == 0xFFFF) {
        ESP_LOGI("LOG", "filter: %s (all lines)", _log_basename(log_filter_file));
    } else {
        ESP_LOGI("LOG", "filter: %s:%d", _log_basename(log_filter_file), (int)log_filter_line);
    }

    // Pro Tag
    for (size_t i = 0; i < kTagCount; i++) {
        esp_log_level_t lvl = esp_log_level_get(kAllTags[i]);
        ESP_LOGI("LOG", "  %-4s = %s", kAllTags[i], levelToStr(lvl));
    }

    ESP_LOGI("LOG", "=========================");
}
