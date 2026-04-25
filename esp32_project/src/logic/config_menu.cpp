/**
 * @file config_menu.cpp
 * @brief Serial Config Menu System — ADR-006 (TASK-038).
 *
 * Einfaches textbasiertes Menu fuer Konfigurationsaenderungen.
 * Nur aktiv im PAUSED-Modus.
 *
 * Hauptmenu:
 *   1-6: Kategorie auswaehlen
 *   S:   Alle speichern
 *   L:   Alle laden
 *   F:   Werkseinstellungen
 *   X:   Menu verlassen
 *
 * Submenu pro Kategorie:
 *   set <key> <value>  — Wert setzen
 *   show                — Werte anzeigen
 *   back                — Zurueck zum Hauptmenu
 *
 * Timeout: 60 Sekunden Inaktivitaet
 */

#include "config_menu.h"
#include "config_framework.h"
#include "config_network.h"
#include "config_ntrip.h"
#include "config_gnss.h"
#include "config_system.h"
#include "op_mode.h"
#include "runtime_config.h"
#include "nvs_config.h"
#include "hal/hal.h"
#include "debug/DebugConsole.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CFG
#include "esp_log.h"
#include "log_ext.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

static const char* TAG = "CFG-MENU";

/// Timeout fuer Inaktivitaet im Menu (60 Sekunden).
static constexpr uint32_t MENU_TIMEOUT_MS = 60000;

/// Maximale Eingabezeilenlaenge.
static constexpr size_t MENU_LINE_MAX = 128;

// ===================================================================
// Hilfsfunktionen
// ===================================================================

/// Prueft ob key ein Passwort-Key ist (fuer Maskierung).
static bool isPasswordKey(const char* key) {
    if (!key) return false;
    return (std::strcmp(key, "pass") == 0 || std::strcmp(key, "password") == 0);
}

/// Liest eine Zeile von Serial mit Timeout.
/// @return Anzahl gelesener Zeichen, oder -1 bei Timeout/Abbruch.
static int menuReadLine(char* buf, size_t buf_size, uint32_t deadline_ms) {
    if (!buf || buf_size == 0) return -1;

    size_t pos = 0;
    buf[0] = '\0';

    while (pos < buf_size - 1) {
        // Timeout-Check
        if (hal_millis() > deadline_ms) {
            DBG.println();
            DBG.println("(Timeout)");
            return -1;
        }

        // Modus-Check: Wenn nicht mehr PAUSED, abbrechen
        if (!opModeIsPaused()) {
            DBG.println();
            DBG.println("(Modus gewechselt — Menu beendet)");
            return -1;
        }

        if (Serial.available() > 0) {
            int ch = Serial.read();

            if (ch == '\r' || ch == '\n') {
                if (pos > 0) {
                    // Zeile abgeschlossen
                    buf[pos] = '\0';
                    DBG.println();
                    return static_cast<int>(pos);
                }
                // Leere Zeile — ignorieren
                continue;
            }

            if (ch == 0x03 || ch == 0x1B) {
                // Ctrl-C oder ESC — Abbruch
                DBG.println("^C");
                return -1;
            }

            if (ch == 0x7F || ch == 0x08) {
                // Backspace
                if (pos > 0) {
                    pos--;
                    DBG.print("\b \b");
                }
                continue;
            }

            // Normales Zeichen
            buf[pos++] = static_cast<char>(ch);

            // Passwort-Maskierung: Sternchen anzeigen statt Zeichen
            if (false) { /* Aktuell keine context-bewusste Maskierung im Hauptmenu */
                DBG.print('*');
            } else {
                DBG.print(static_cast<char>(ch));
            }
        } else {
            delay(10);  // kurze Pause um CPU-Last zu reduzieren
        }
    }

    buf[buf_size - 1] = '\0';
    DBG.println();
    return static_cast<int>(pos);
}

// ===================================================================
// Hauptmenu
// ===================================================================

static void menuPrintMain(void) {
    DBG.println();
    DBG.println("╔══════════════════════════════════════════╗");
    DBG.println("║     AgSteer Konfigurations-Menu          ║");
    DBG.println("║     (PAUSED Modus)                       ║");
    DBG.println("╠══════════════════════════════════════════╣");
    DBG.println("║ 1 — Netzwerk                            ║");
    DBG.println("║ 2 — NTRIP                               ║");
    DBG.println("║ 3 — GNSS                                ║");
    DBG.println("║ 4 — PID Regler                          ║");
    DBG.println("║ 5 — Aktuator                            ║");
    DBG.println("║ 6 — System                              ║");
    DBG.println("╠══════════════════════════════════════════╣");
    DBG.println("║ S — Alle speichern                      ║");
    DBG.println("║ L — Alle laden                          ║");
    DBG.println("║ F — Werkseinstellungen (Factory Reset)   ║");
    DBG.println("║ X — Menu verlassen                      ║");
    DBG.println("╚══════════════════════════════════════════╝");
    DBG.print("> ");
}

/// Kategorie-Submenu.
static void menuCategory(const ConfigCategoryOps* ops) {
    if (!ops) return;

    DBG.println();
    DBG.printf("=== %s ===\n", ops->name);
    DBG.println("  set <key> <value>  — Wert aendern");
    DBG.println("  show               — Werte anzeigen");
    DBG.println("  back               — Zurueck");

    char line[MENU_LINE_MAX];
    uint32_t deadline = hal_millis() + MENU_TIMEOUT_MS;

    while (true) {
        // Timeout-Check
        if (hal_millis() > deadline) {
            DBG.println("(Timeout)");
            return;
        }
        if (!opModeIsPaused()) {
            DBG.println("(Modus gewechselt)");
            return;
        }

        DBG.print("  > ");
        int len = menuReadLine(line, sizeof(line), deadline);
        if (len <= 0) return;

        // Deadline nach jeder Eingabe zuruecksetzen
        deadline = hal_millis() + MENU_TIMEOUT_MS;

        // Tokenize
        char* argv[4] = {};
        int argc = 0;
        char* token = std::strtok(line, " \t");
        while (token && argc < 4) {
            argv[argc++] = token;
            token = std::strtok(nullptr, " \t");
        }

        if (argc == 0) continue;

        if (std::strcmp(argv[0], "back") == 0 || std::strcmp(argv[0], "exit") == 0) {
            return;
        }

        if (std::strcmp(argv[0], "show") == 0) {
            if (ops->show) {
                ops->show(static_cast<ConfigStream>(&DBG));
            } else {
                DBG.println("  (keine Show-Funktion)");
            }
            continue;
        }

        if (std::strcmp(argv[0], "set") == 0) {
            if (argc < 3) {
                DBG.println("  usage: set <key> <value>");
                continue;
            }

            if (!ops->set) {
                DBG.println("  (keine Set-Funktion)");
                continue;
            }

            // Passwort-Maskierung beim Echo
            if (isPasswordKey(argv[1])) {
                DBG.printf("  %s = ********\n", argv[1]);
            } else {
                DBG.printf("  %s = %s\n", argv[1], argv[2]);
            }

            if (ops->set(argv[1], argv[2])) {
                DBG.println("  OK");
            } else {
                DBG.println("  FEHLER — ungueltiger Key oder Wert");
            }
            continue;
        }

        DBG.printf("  Unbekannt: %s\n", argv[0]);
    }
}

// ===================================================================
// Public API
// ===================================================================

void configMenuInit(void) {
    LOGI(TAG, "initialisiert");
}

void configMenuShow(ConfigStream output) {
    auto* out = static_cast<Stream*>(output);
    if (!out) return;

    if (!opModeIsPaused()) {
        out->println("Fehler: Config nur im PAUSED Modus verfuegbar.");
        out->println("  'mode paused' um in den PAUSED Modus zu wechseln.");
        return;
    }

    out->println();
    out->println("=== Config Menu (PAUSED Mode) ===");
    configFrameworkPrintStatus(output);
    out->println();
    out->println("Befehle:");
    out->println("  config               — Interaktives Menu starten");
    out->println("  config show [kat]    — Werte anzeigen");
    out->println("  config save          — Alle in NVS speichern");
    out->println("  config load          — Aus NVS laden");
    out->println("  config factory confirm — Werkseinstellungen");
    out->println();
}

void configMenuStart(void) {
    if (!opModeIsPaused()) {
        DBG.println("Fehler: Config Menu nur im PAUSED Modus verfuegbar.");
        DBG.println("  'mode paused' um in den PAUSED Modus zu wechseln.");
        return;
    }

    LOGI(TAG, "Interaktives Menu gestartet");
    hal_log("CFG-MENU: gestartet");

    char line[MENU_LINE_MAX];
    uint32_t deadline = hal_millis() + MENU_TIMEOUT_MS;

    while (true) {
        menuPrintMain();

        int len = menuReadLine(line, sizeof(line), deadline);
        if (len <= 0) break;

        // Timeout-Check
        if (hal_millis() > deadline) {
            DBG.println("(Timeout)");
            break;
        }
        if (!opModeIsPaused()) {
            DBG.println("(Modus gewechselt — Menu beendet)");
            break;
        }

        // Deadline nach jeder Eingabe zuruecksetzen
        deadline = hal_millis() + MENU_TIMEOUT_MS;

        // Einzelnes Zeichen interpretieren
        char cmd = line[0];
        if (std::strlen(line) > 1) cmd = '\0';  // Nur Einzelzeichen-Befehle

        const ConfigCategoryOps* cat_ops = nullptr;

        switch (cmd) {
            case '1': cat_ops = configFrameworkFindCategory(CONFIG_CAT_NETWORK);  break;
            case '2': cat_ops = configFrameworkFindCategory(CONFIG_CAT_NTRIP);    break;
            case '3': cat_ops = configFrameworkFindCategory(CONFIG_CAT_GNSS);     break;
            case '4': cat_ops = configFrameworkFindCategory(CONFIG_CAT_PID);      break;
            case '5': cat_ops = configFrameworkFindCategory(CONFIG_CAT_ACTUATOR); break;
            case '6': cat_ops = configFrameworkFindCategory(CONFIG_CAT_SYSTEM);   break;
            case 's': case 'S':
                if (configFrameworkSaveAll()) {
                    DBG.println("Alle Konfigurationen gespeichert.");
                } else {
                    DBG.println("FEHLER beim Speichern (Validierung fehlgeschlagen).");
                }
                continue;
            case 'l': case 'L':
                {
                    RuntimeConfig& cfg = softConfigGet();
                    nvsConfigLoad(cfg);
                    DBG.println("Konfiguration aus NVS geladen.");
                }
                continue;
            case 'f': case 'F':
                DBG.print("Werkseinstellungen? Alle Daten werden geloescht! (ja/nein): ");
                {
                    char confirm[MENU_LINE_MAX];
                    int clen = menuReadLine(confirm, sizeof(confirm), hal_millis() + 10000);
                    if (clen > 0 && (std::strcmp(confirm, "ja") == 0 || std::strcmp(confirm, "yes") == 0)) {
                        configFrameworkFactoryReset();
                        DBG.println("NVS geloescht. Neustart empfohlen (Restart).");
                        DBG.flush();
                        ESP.restart();
                    } else {
                        DBG.println("Abgebrochen.");
                    }
                }
                continue;
            case 'x': case 'X':
                DBG.println("Menu beendet.");
                break;
            default:
                DBG.printf("Unbekannt: %s\n", line);
                continue;
        }

        // 'x' bricht die Schleife
        if (cmd == 'x' || cmd == 'X') break;

        // Kategorie-Submenu
        if (cat_ops) {
            menuCategory(cat_ops);
        }
    }

    LOGI(TAG, "Interaktives Menu beendet");
}
