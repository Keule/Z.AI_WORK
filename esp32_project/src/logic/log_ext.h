/**
 * @file log_ext.h
 * @brief Runtime-Logging-Wrapper auf Basis von esp_log mit Datei:Zeile Kennung.
 *
 * Verwendung in .cpp Datei (WICHTIG: Reihenfolge einhalten!):
 *
 *   #include "log_config.h"
 *   #define LOG_LOCAL_LEVEL LOG_LEVEL_NET
 *   #include "esp_log.h"
 *   #include "log_ext.h"
 *
 *   LOGD("NET", "PGN %d received", pgn);
 *   LOGI("NET", "Hello from AgIO");
 *   LOGW("NET", "unhandled PGN 0x%02X", pgn);
 *   LOGE("NET", "CRC mismatch: got 0x%02X", crc);
 *
 * Ausgabeformat (Beispiel):
 *   I (210) NET: [net.cpp:47] PGN 252 received (8 bytes)
 *   E (315) NET: [net.cpp:83] CRC mismatch: got 0x47
 *
 *   I = Level, 210 = ms seit Boot, NET = Tag, net.cpp:47 = Datei:Zeile
 *
 * Serial-Kommandos (zur Laufzeit):
 *   log <tag> <level>       z.B. "log net debug"
 *   log all <level>         z.B. "log all info"
 *   log status              Aktuelle Konfiguration anzeigen
 *   filter <file:line>      z.B. "filter net.cpp:47"
 *   filter <file>           z.B. "filter net.cpp"  (ganze Datei)
 *   filter off              Filter aufheben
 */

#pragma once

#include "esp_log.h"
#include <cstring>

// ===================================================================
// Laufzeit-Filter-Zustand (definiert in log_ext.cpp)
// ===================================================================
extern uint16_t log_filter_line;       // 0 = kein Filter
extern char     log_filter_file[64];   // Dateiname fuer Filter

// ===================================================================
// Intern: Dateiname aus Pfad extrahieren
// ===================================================================
static inline const char* _log_basename(const char* path) {
    const char* s = path;
    const char* last = path;
    while (*s) {
        if (*s == '/' || *s == '\\') last = s + 1;
        s++;
    }
    return last;
}

// ===================================================================
// Intern: Zeilen-Filter-Pruefung
// ===================================================================
#if LOG_FILTER_ENABLED
static inline bool _log_line_allowed(const char* file, int line) {
    if (log_filter_line == 0) return true;
    const char* base = _log_basename(file);
    const char* fbase = _log_basename(log_filter_file);
    if (std::strcmp(base, fbase) != 0) return false;
    if (log_filter_line != 0xFFFF && line != (int)log_filter_line) return false;
    return true;
}
#else
static inline bool _log_line_allowed(const char*, int) { return true; }
#endif

// ===================================================================
// Log-Makros mit Datei:Zeile Kennung
// ===================================================================
// Verwendung: LOGD("TAG", "format %d", value)

#define LOGD(tag, fmt, ...) do { \
    if (_log_line_allowed(__FILE__, __LINE__)) \
        ESP_LOGD(tag, "[%s:%d] " fmt, \
                 _log_basename(__FILE__), __LINE__, ##__VA_ARGS__); \
} while(0)

#define LOGI(tag, fmt, ...) do { \
    if (_log_line_allowed(__FILE__, __LINE__)) \
        ESP_LOGI(tag, "[%s:%d] " fmt, \
                 _log_basename(__FILE__), __LINE__, ##__VA_ARGS__); \
} while(0)

#define LOGW(tag, fmt, ...) do { \
    if (_log_line_allowed(__FILE__, __LINE__)) \
        ESP_LOGW(tag, "[%s:%d] " fmt, \
                 _log_basename(__FILE__), __LINE__, ##__VA_ARGS__); \
} while(0)

#define LOGE(tag, fmt, ...) do { \
    if (_log_line_allowed(__FILE__, __LINE__)) \
        ESP_LOGE(tag, "[%s:%d] " fmt, \
                 _log_basename(__FILE__), __LINE__, ##__VA_ARGS__); \
} while(0)

#define LOGV(tag, fmt, ...) do { \
    if (_log_line_allowed(__FILE__, __LINE__)) \
        ESP_LOGV(tag, "[%s:%d] " fmt, \
                 _log_basename(__FILE__), __LINE__, ##__VA_ARGS__); \
} while(0)

// ===================================================================
// Serial-Kommando-Handler (nur wenn LOG_SERIAL_CMD = 1)
// ===================================================================
#if LOG_SERIAL_CMD

/// Verarbeitet ein Serial-Kommando fuer die Log-Steuerung.
/// Aufrufen aus loop() wenn Serial Daten verfuegbar.
void logProcessSerialCmd(const char* cmd);

/// Gibt aktuelle Log-Konfiguration auf Serial aus.
void logPrintStatus(void);

#endif // LOG_SERIAL_CMD
