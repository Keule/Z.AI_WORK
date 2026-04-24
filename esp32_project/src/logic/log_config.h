/**
 * @file log_config.h
 * @brief Zentrale Logging-Konfiguration – EINE Datei steuert alles.
 *
 * Compilezeit:
 *   LOG_LEVEL_xxx steuert, welche ESP_LOGx Aufrufe in der
 *   jeweiligen .cpp kompiliert werden. Alles unterhalb ist null Overhead.
 *   In der .cpp Datei verwenden:
 *     #include "log_config.h"
 *     #define LOG_LOCAL_LEVEL LOG_LEVEL_NET
 *     #include "esp_log.h"
 *     #include "log_ext.h"
 *
 * Laufzeit:
 *   esp_log_level_set("NET", ESP_LOG_DEBUG) pro Modul.
 *   "filter net.cpp:47" fuer Zeilen-Filter.
 *
 * Aenderungen hier wirken sich auf ALLE Module aus.
 */

#pragma once

// ── Log-Level numerisch (muss mit esp_log.h uebereinstimmen) ──
#define LOG_LVL_NONE     0
#define LOG_LVL_ERROR    1
#define LOG_LVL_WARN     2
#define LOG_LVL_INFO     3
#define LOG_LVL_DEBUG    4
#define LOG_LVL_VERBOSE  5

// ── Compilezeit: Loglevel pro Modul ──
// Alles unterhalb dieses Levels wird komplett wegkompiliert (0 Code, 0 RAM).
// 0=NONE  1=ERROR  2=WARN  3=INFO  4=DEBUG  5=VERBOSE
#define LOG_LEVEL_MAIN   LOG_LVL_DEBUG
#define LOG_LEVEL_NET    LOG_LVL_DEBUG
#define LOG_LEVEL_CTL    LOG_LVL_DEBUG
#define LOG_LEVEL_MOD    LOG_LVL_INFO
#define LOG_LEVEL_HWS    LOG_LVL_INFO
#define LOG_LEVEL_PGN    LOG_LVL_DEBUG
#define LOG_LEVEL_WAS    LOG_LVL_INFO
#define LOG_LEVEL_IMU    LOG_LVL_INFO
#define LOG_LEVEL_ACT    LOG_LVL_INFO
#define LOG_LEVEL_SDL    LOG_LVL_INFO
#define LOG_LEVEL_CFG    LOG_LVL_DEBUG
#define LOG_LEVEL_MAINT  LOG_LVL_INFO
#define LOG_LEVEL_OTA    LOG_LVL_INFO
#define LOG_LEVEL_HAL    LOG_LVL_DEBUG
#define LOG_LEVEL_NTRIP  LOG_LVL_DEBUG

// ── Laufzeit-Features (auf 0 setzen spart RAM) ──
#define LOG_FILTER_ENABLED   1   // "filter net.cpp:47" Kommando
#define LOG_SERIAL_CMD       1   // Serial-Eingabe "log ..." verarbeiten

// Compile-time diagnostic print intervals.
// Override via PlatformIO build_flags, e.g. -DLOG_IMU_DIAG_INTERVAL_MS=100.
// Set to 0 to disable the corresponding periodic diagnostic log.
#ifndef LOG_IMU_DIAG_INTERVAL_MS
#define LOG_IMU_DIAG_INTERVAL_MS 500
#endif

#ifndef LOG_WAS_DIAG_INTERVAL_MS
#define LOG_WAS_DIAG_INTERVAL_MS 0
#endif

#ifndef LOG_SPI_TIMING_INTERVAL_MS
#define LOG_SPI_TIMING_INTERVAL_MS 5000
#endif
