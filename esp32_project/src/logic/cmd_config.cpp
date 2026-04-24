/**
 * @file cmd_config.cpp
 * @brief CLI config command — menu, show, set, get, save, load, factory, status.
 *
 * Split from cli.cpp (Step 7).
 */

#include "cli.h"
#include "runtime_config.h"
#include "nvs_config.h"
#include "config_framework.h"
#include "config_menu.h"
#include "op_mode.h"

#include <Arduino.h>
#include <esp_system.h>

#include <cstdio>
#include <cstring>

extern Stream* s_cli_out;

namespace {

void cliCmdSave(int, char**) {
    RuntimeConfig& cfg = softConfigGet();
    if (nvsConfigSave(cfg)) {
        s_cli_out->println("Config saved to NVS.");
        s_cli_out->println("WARNING: ntrip_password is stored in plaintext for now.");
    } else {
        s_cli_out->println("ERROR: failed to save config to NVS.");
    }
}

void cliCmdLoad(int, char**) {
    RuntimeConfig& cfg = softConfigGet();
    nvsConfigLoad(cfg);
    s_cli_out->println("Config loaded from NVS.");
}

void cliCmdFactory(int argc, char** argv) {
    if (argc < 2 || std::strcmp(argv[1], "confirm") != 0) {
        s_cli_out->println("WARNING: This will erase all saved configuration.");
        s_cli_out->println("Run: factory confirm");
        return;
    }

    nvsConfigFactoryReset();
    s_cli_out->println("NVS erased. Restarting with defaults...");
    s_cli_out->flush();
    ESP.restart();
}

void cliCmdConfig(int argc, char** argv) {
    if (argc < 2) {
        configMenuShow(static_cast<ConfigStream>(s_cli_out));
        return;
    }

    if (std::strcmp(argv[1], "menu") == 0) {
        configMenuStart();
        return;
    }

    if (std::strcmp(argv[1], "show") == 0) {
        if (argc >= 3) {
            const ConfigCategoryOps* ops = configFrameworkFindCategoryByName(argv[2]);
            if (ops) {
                configFrameworkShowCategory(ops->category, static_cast<ConfigStream>(s_cli_out));
            } else {
                s_cli_out->printf("Unbekannte Kategorie: %s\n", argv[2]);
                configFrameworkPrintStatus(static_cast<ConfigStream>(s_cli_out));
            }
        } else {
            configFrameworkShowAll(static_cast<ConfigStream>(s_cli_out));
        }
        return;
    }

    if (std::strcmp(argv[1], "set") == 0) {
        if (!configFrameworkIsEditable()) {
            s_cli_out->println("Fehler: Config nur im PAUSED Modus");
            return;
        }
        if (argc < 5) {
            s_cli_out->println("usage: config set <category> <key> <value>");
            s_cli_out->println("  Kategorien: network, ntrip, gnss, pid, actuator, system");
            return;
        }
        const ConfigCategoryOps* ops = configFrameworkFindCategoryByName(argv[2]);
        if (!ops) {
            s_cli_out->printf("Unbekannte Kategorie: %s\n", argv[2]);
            return;
        }
        if (std::strcmp(argv[3], "pass") == 0 || std::strcmp(argv[3], "password") == 0) {
            s_cli_out->printf("  %s.%s = ********\n", ops->name, argv[3]);
        } else {
            s_cli_out->printf("  %s.%s = %s\n", ops->name, argv[3], argv[4]);
        }
        if (configFrameworkSet(ops->category, argv[3], argv[4])) {
            s_cli_out->println("OK");
        } else {
            s_cli_out->println("FEHLER — ungueltiger Key oder Wert");
        }
        return;
    }

    if (std::strcmp(argv[1], "get") == 0) {
        if (argc < 4) {
            s_cli_out->println("usage: config get <category> <key>");
            return;
        }
        const ConfigCategoryOps* ops = configFrameworkFindCategoryByName(argv[2]);
        if (!ops) {
            s_cli_out->printf("Unbekannte Kategorie: %s\n", argv[2]);
            return;
        }
        char val_buf[64] = {};
        if (configFrameworkGet(ops->category, argv[3], val_buf, sizeof(val_buf))) {
            s_cli_out->printf("  %s.%s = %s\n", ops->name, argv[3], val_buf);
        } else {
            s_cli_out->printf("FEHLER — Key '%s' nicht gefunden\n", argv[3]);
        }
        return;
    }

    if (std::strcmp(argv[1], "save") == 0) {
        if (!opModeIsPaused()) {
            s_cli_out->println("Fehler: Config nur im PAUSED Modus");
            return;
        }
        if (configFrameworkSaveAll()) {
            s_cli_out->println("Alle Konfigurationen gespeichert.");
        } else {
            s_cli_out->println("Fehler beim Speichern (Validierung fehlgeschlagen).");
        }
        return;
    }

    if (std::strcmp(argv[1], "load") == 0) {
        RuntimeConfig& cfg = softConfigGet();
        nvsConfigLoad(cfg);
        s_cli_out->println("Konfiguration aus NVS geladen.");
        return;
    }

    if (std::strcmp(argv[1], "factory") == 0) {
        if (argc < 3 || std::strcmp(argv[2], "confirm") != 0) {
            s_cli_out->println("WARNING: Werkseinstellungen loeschen alle NVS Daten.");
            s_cli_out->println("  config factory confirm");
            return;
        }
        if (!opModeIsPaused()) {
            s_cli_out->println("Fehler: Factory Reset nur im PAUSED Modus");
            return;
        }
        configFrameworkFactoryReset();
        s_cli_out->println("NVS geloescht. Neustart...");
        s_cli_out->flush();
        ESP.restart();
        return;
    }

    if (std::strcmp(argv[1], "status") == 0) {
        configFrameworkPrintStatus(static_cast<ConfigStream>(s_cli_out));
        return;
    }

    s_cli_out->println("usage: config [menu|show|set|get|save|load|factory|status]");
}

}  // namespace

void cmd_config_register(void) {
    (void)cliRegisterCommand("save",    &cliCmdSave,    "Save runtime config to NVS");
    (void)cliRegisterCommand("load",    &cliCmdLoad,    "Load runtime config from NVS");
    (void)cliRegisterCommand("factory", &cliCmdFactory, "Factory reset (use: factory confirm)");
    (void)cliRegisterCommand("config",  &cliCmdConfig,  "Config framework (ADR-006)");
}
