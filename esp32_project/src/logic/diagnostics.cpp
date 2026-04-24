/**
 * @file diagnostics.cpp
 * @brief Erweiterte Diagnosefunktionen fuer PAUSED Modus — TASK-041/043.
 *
 * Bietet:
 *   - Umfassender Selftest aller Module (ETH, IMU, ADS, ACT, SD, GNSS, NTRIP)
 *   - Modul-Status-Tabelle (compiled/detected/active/pins/deps)
 *   - Pin-Claim-Map (GPIO -> Owner)
 *   - SD-Log Statistiken
 *   - SD-Log CSV Export ueber Serial
 */

#include "diagnostics.h"
#include "hal/hal.h"
#include "module_interface.h"
#include "features.h"
#include "sd_logger.h"

#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
#include "ntrip.h"
#endif

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CFG
#include "esp_log.h"
#include "log_ext.h"

// Forward declaration (defined in sd_logger.cpp with extern "C" linkage)
extern "C" uint32_t sdLoggerGetOverflowCount(void);

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "fw_config.h"
#endif

static const char* TAG = "DIAG";

// ===================================================================
// Selftest
// ===================================================================

static void addModuleResult(DiagSelftestResult& result,
                            const char* name, bool tested, bool passed,
                            const char* detail = nullptr) {
    if (result.count >= DIAG_MAX_MODULES) return;
    DiagModuleResult& m = result.modules[result.count++];
    m.name = name;
    m.tested = tested;
    m.passed = passed;
    m.detail = detail;
    if (tested) {
        if (passed) result.passed++;
        else result.failed++;
    }
}

DiagSelftestResult diagRunSelftest(void) {
    DiagSelftestResult result = {};
    memset(&result, 0, sizeof(result));

    // ETH Test
    {
        const auto* ops = moduleSysOps(ModuleId::ETH);
        bool compiled = ops && ops->is_enabled();
        bool active = moduleSysIsActive(ModuleId::ETH);
        bool link = hal_net_link_up();
        bool connected = hal_net_is_connected();
        bool detected = hal_net_detected();
        bool ok = compiled && detected && link && connected;
        addModuleResult(result, "ETH", compiled, ok,
            ok ? nullptr : (detected ? (link ? "IP nicht bezogen" : "Link DOWN") : "nicht detektiert"));
    }

    // IMU Test
    {
        const auto* ops = moduleSysOps(ModuleId::IMU);
        bool compiled = ops && ops->is_enabled();
        bool active = moduleSysIsActive(ModuleId::IMU);
        const auto* mod = moduleSysGet(ModuleId::IMU);
        bool detected = mod ? mod->state.detected : false;
        bool ok = compiled && detected && active;
        addModuleResult(result, "IMU", compiled, ok,
            ok ? nullptr : (compiled ? (detected ? "nicht aktiv" : "HW nicht detektiert") : "nicht kompiliert"));
    }

    // ADS/WAS Test
    {
        const auto* ops = moduleSysOps(ModuleId::WAS);
        bool compiled = ops && ops->is_enabled();
        bool active = moduleSysIsActive(ModuleId::WAS);
        const auto* mod = moduleSysGet(ModuleId::WAS);
        bool detected = mod ? mod->state.detected : false;
        bool ok = compiled && detected && active;
        addModuleResult(result, "ADS/WAS", compiled, ok,
            ok ? nullptr : (compiled ? (detected ? "nicht aktiv" : "HW nicht detektiert") : "nicht kompiliert"));
    }

    // ACT Test
    {
        const auto* ops = moduleSysOps(ModuleId::ACTUATOR);
        bool compiled = ops && ops->is_enabled();
        bool active = moduleSysIsActive(ModuleId::ACTUATOR);
        const auto* mod = moduleSysGet(ModuleId::ACTUATOR);
        bool detected = mod ? mod->state.detected : false;
        bool ok = compiled && detected && active;
        addModuleResult(result, "ACT", compiled, ok,
            ok ? nullptr : (compiled ? (detected ? "nicht aktiv" : "HW nicht detektiert") : "nicht kompiliert"));
    }

    // SD Test
    {
        const auto* ops = moduleSysOps(ModuleId::LOGGING);
        bool compiled = ops && ops->is_enabled();
        bool active = moduleSysIsActive(ModuleId::LOGGING);
        bool card_present = hal_sd_card_present();
        bool ok = compiled && active && card_present;
        addModuleResult(result, "SD", compiled, ok,
            ok ? nullptr : (compiled ? (active ? "Keine Karte" : "nicht aktiv") : "nicht kompiliert"));
    }

    // GNSS Test
    {
        const auto* ops = moduleSysOps(ModuleId::GNSS);
        bool compiled = ops && ops->is_enabled();
        bool active = moduleSysIsActive(ModuleId::GNSS);
        bool uart0_ready = hal_gnss_uart_is_ready(0);
        bool uart1_ready = hal_gnss_uart_is_ready(1);
        bool ok = compiled && active && (uart0_ready || uart1_ready);
        char detail_buf[64] = {};
        if (compiled && active && !uart0_ready && !uart1_ready) {
            snprintf(detail_buf, sizeof(detail_buf), "UART nicht bereit (0=%d, 1=%d)",
                     uart0_ready, uart1_ready);
        }
        addModuleResult(result, "GNSS", compiled, ok,
            ok ? nullptr : (compiled ? (active ? detail_buf : "nicht aktiv") : "nicht kompiliert"));
    }

    // NTRIP Test
#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
    {
        const auto* ops = moduleSysOps(ModuleId::NTRIP);
        bool compiled = ops && ops->is_enabled();
        bool active = moduleSysIsActive(ModuleId::NTRIP);
        bool conn_ok = false;
        const char* detail = nullptr;
        if (compiled && active) {
            NtripState ns = ntripGetState();
            conn_ok = (ns.conn_state == NtripConnState::CONNECTED);
            if (!conn_ok) {
                switch (ns.conn_state) {
                    case NtripConnState::IDLE:          detail = "IDLE (kein Host)"; break;
                    case NtripConnState::CONNECTING:    detail = "Verbinde..."; break;
                    case NtripConnState::AUTHENTICATING:detail = "Auth..."; break;
                    case NtripConnState::DISCONNECTED:  detail = "Getrennt"; break;
                    case NtripConnState::ERROR:         detail = ns.last_error[0] ? ns.last_error : "Fehler"; break;
                    default:                            detail = "Unbekannt"; break;
                }
            }
        }
        addModuleResult(result, "NTRIP", compiled, active ? conn_ok : false,
            compiled && active ? detail : (compiled ? "nicht aktiv" : "nicht kompiliert"));
    }
#else
    addModuleResult(result, "NTRIP", false, false, "nicht kompiliert");
#endif

    // Safety Test
    {
        const auto* ops = moduleSysOps(ModuleId::SAFETY);
        bool compiled = ops && ops->is_enabled();
        bool active = moduleSysIsActive(ModuleId::SAFETY);
        bool safety = hal_safety_ok();
        bool ok = compiled && active && safety;
        addModuleResult(result, "SAFETY", compiled, ok,
            ok ? nullptr : (compiled ? (active ? "KICK (Safety LOW)" : "nicht aktiv") : "nicht kompiliert"));
    }

    LOGI(TAG, "Selftest abgeschlossen: %d/%d bestanden, %d fehlgeschlagen",
         result.passed, result.count, result.failed);

    return result;
}

// ===================================================================
// Module Status
// ===================================================================

void diagPrintModuleStatus(void* output_stream) {
    auto* out = static_cast<Stream*>(output_stream);
    if (!out) out = &Serial;

    out->println("=== Module Status ===");
    out->printf("  %-8s Active   Detected  Healthy  Error\n", "Module");

    for (int i = 0; i < static_cast<int>(ModuleId::COUNT); i++) {
        auto id = static_cast<ModuleId>(i);
        const auto* mod = moduleSysGet(id);
        if (!mod) continue;
        const char* name = moduleIdToName(id);
        out->printf("  %-8s %s  %s     %s       %ld\n",
                    name,
                    mod->active ? "ON " : "OFF",
                    mod->state.detected ? "ja " : "nein",
                    moduleSysIsHealthy(id, hal_millis()) ? "ja " : "nein",
                    (long)mod->state.error_code);
    }
}

// ===================================================================
// Pin Map
// ===================================================================

/// Maximale GPIO-Pinnummer fuer ESP32-S3.
static constexpr int GPIO_MAX = 48;

void diagPrintPinMap(void* output_stream) {
    auto* out = static_cast<Stream*>(output_stream);
    if (!out) out = &Serial;

    out->println("=== Pin-Claim Map ===");

    int claimed_count = 0;
    for (int pin = 0; pin <= GPIO_MAX; pin++) {
        const char* owner = hal_pin_claim_owner(pin);
        if (owner && owner[0] != '\0') {
            out->printf("  GPIO %2d -> %s\n", pin, owner);
            claimed_count++;
        }
    }

    if (claimed_count == 0) {
        out->println("  (keine Pins beansprucht)");
    } else {
        out->printf("  --- %d Pins belegt ---\n", claimed_count);
    }
}

// ===================================================================
// SD-Log Statistiken
// ===================================================================

void diagPrintLogStats(void) {
    Serial.println("=== SD-Log Statistiken ===");

    if (!moduleSysIsActive(ModuleId::LOGGING)) {
        Serial.println("  SD Modul nicht aktiv");
        return;
    }

    uint32_t flushed = sdLoggerGetRecordsFlushed();
    uint32_t buf_count = sdLoggerGetBufferCount();
    uint32_t overflow = sdLoggerGetOverflowCount();

    bool psram = sdLoggerPsramBufferActive();
    uint32_t psram_count = sdLoggerPsramBufferCount();

    Serial.printf("  Gesamt geschrieben:    %lu Records\n", (unsigned long)flushed);
    Serial.printf("  Buffer Fuellstand:      %lu Records\n", (unsigned long)buf_count);
    Serial.printf("  Buffer Overflows:       %lu\n", (unsigned long)overflow);
    Serial.printf("  PSRAM Buffer:           %s\n", psram ? "AKTIV" : "nicht aktiv");
    if (psram) {
        Serial.printf("  PSRAM Buffer Count:     %lu Records\n", (unsigned long)psram_count);
    }
    Serial.printf("  Logging aktiv:          %s\n", sdLoggerIsActive() ? "JA" : "NEIN");
}

// ===================================================================
// SD-Log CSV Export
// ===================================================================

bool diagExportLogCsv(void) {
    // Nur im PAUSED Modus zulaessig!
    if (modeGet() != OpMode::CONFIG) {
        Serial.println("Fehler: CSV Export nur im PAUSED Modus zulaessig");
        return false;
    }

    if (!moduleSysIsActive(ModuleId::LOGGING)) {
        Serial.println("Fehler: SD Modul nicht aktiv");
        return false;
    }

    Serial.println("SD Log CSV Export gestartet...");

#if defined(ARDUINO_ARCH_ESP32)
    // SPI Bus freigeben (Sensor-SPI deinit)
    hal_sensor_spi_deinit();
    hal_delay_ms(10);

    SPIClass sdSPI(SD_SPI_BUS);
    sdSPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS);

    bool result = false;

    if (!SD.begin(SD_CS, sdSPI, 4000000, "/sd", 5)) {
        Serial.println("Fehler: SD Karte nicht verfuegbar");
        goto cleanup;
    }

    // Neueste Log-Datei finden
    {
        char path[32];
        int latest = -1;
        for (int i = 999; i >= 1; i--) {
            snprintf(path, sizeof(path), "/log_%03d.csv", i);
            if (SD.exists(path)) {
                latest = i;
                break;
            }
        }

        if (latest < 0) {
            Serial.println("Keine Log-Dateien gefunden");
            goto cleanup;
        }

        snprintf(path, sizeof(path), "/log_%03d.csv", latest);
        File f = SD.open(path, FILE_READ);
        if (!f) {
            Serial.printf("Fehler: %s nicht lesbar\n", path);
            goto cleanup;
        }

        Serial.printf("--- %s ---\n", path);
        // Datei zeilenweise ueber Serial streamen
        char line[256];
        while (f.available()) {
            int len = f.readBytesUntil('\n', line, sizeof(line) - 1);
            if (len <= 0) break;
            line[len] = '\0';
            Serial.println(line);

            // Pausen vermeiden Serial-Buffer-Ueberlauf
            if (Serial.availableForWrite() < 128) {
                Serial.flush();
                delay(5);
            }
        }
        f.close();
        Serial.println("--- EOF ---");
        Serial.printf("Export abgeschlossen: %s\n", path);
        result = true;
    }

cleanup:
    SD.end();
    sdSPI.end();
    hal_sensor_spi_reinit();
    return result;

#else
    Serial.println("Fehler: SD Export nicht auf dieser Plattform verfuegbar");
    return false;
#endif
}
