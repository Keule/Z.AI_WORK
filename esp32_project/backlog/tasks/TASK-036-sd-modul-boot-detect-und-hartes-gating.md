# TASK-036 SD-Modul mit Boot-Detect und hartem Gating

- **ID**: TASK-036
- **Titel**: SD-Funktionalität als Modul aktivierbar machen und bei fehlender Karte beim Boot vollständig deaktivieren
- **Status**: done
- **Priorität**: high
- **Komponenten**: `src/logic/modules.cpp`, `src/logic/modules.h`, `src/main.cpp`, `src/hal/hal.h`, `src/hal_esp32/hal_impl.cpp`, `src/hal_esp32/sd_logger_esp32.cpp`, `src/hal_esp32/sd_ota_esp32.cpp`, `src/logic/runtime_config.cpp`, `include/board_profile/*`
- **Dependencies**: TASK-027, TASK-028
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer
- **Epic**: EPIC-003

- **classification**: dependent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Origin**:
  Nutzeranforderung aus Chat (2026-04-21):
  1. SD soll genauso wie ADS/IMU/NTRIP als Modul aktivierbar/deaktivierbar sein.
  2. Beim Boot soll über einen SD-Pin geprüft werden, ob eine Karte eingelegt ist.
  3. Wenn keine Karte erkannt wird, soll die komplette SD-Funktionalität deaktiviert bleiben.

- **Diskussion**:
  - Direkt: https://chatgpt.com/codex/cloud/tasks/task_e_69e7aadc724c8325bde97fdc35e7544d

- **Kontext/Problem**:
  Das bestehende Feature-Modulsystem modelliert derzeit `IMU`, `ADS`, `ACT`, `ETH`, `GNSS`,
  `NTRIP`, `SAFETY`, `LOGSW`, aber kein dediziertes `SD`-Modul. SD-Pfade (Logger, OTA,
  runtime_config-Credentials) greifen lokal auf SD zu. Dadurch gibt es keinen zentralen
  Runtime-Zustand „SD verfügbar/aktiv“ und kein einheitliches Boot-Gating per SD-Detect.

- **Scope (in)**:
  - Neues Feature-Modul `MOD_SD` in der Modul-API einführen (`FirmwareFeatureId` + `MOD_COUNT`).
  - `g_features[]`, owner-tags und Initialisierungslogik (`featureModulesInitCompiled`,
    `featureModulesSyncHwDetected`) um SD erweitern.
  - Board-Profile um SD-Detect-Definition ergänzen (`SD_DETECT_PIN`) sowie `FEAT_PINS_SD`
    und `FEAT_PINS_SD_COUNT`.
  - Neue HAL-Funktion für SD-Präsenzcheck einführen (z. B. `hal_sd_card_present()`), inkl.
    dokumentierter active-low/high Polarität.
  - Boot-Reihenfolge in `main.cpp`: SD-Modul früh aktivieren/prüfen, bevor OTA/Logger/RuntimeConfig
    SD-Zugriffe ausführen.
  - Hartes Gating aller SD-Pfade über `moduleIsActive(MOD_SD)`:
    - `sd_logger_esp32.cpp`
    - `sd_ota_esp32.cpp`
    - `runtime_config.cpp` (SD-basiertes Laden)
  - Einheitliche Logs für „SD nicht vorhanden → SD-Funktionen deaktiviert“.

- **Nicht-Scope (out)**:
  - Keine PGN-Protokolländerungen.
  - Kein Umbau des Task-Modells (`controlTask/commTask/maintTask`).
  - Keine neue Benutzeroberfläche für Modulumschaltung.

- **Pflichtlektüre vor Umsetzung**:
  1. `README.md`
  2. `agents.md`
  3. `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  4. `docs/adr/subsystems/ADR-LOG-001-logging-buffering-and-sd-flush-policy.md`
  5. dieser Task (`TASK-036`)

- **files_read**:
  - `src/logic/modules.h`
  - `src/logic/modules.cpp`
  - `include/soft_config.h`
  - `src/main.cpp`
  - `src/hal/hal.h`
  - `src/hal_esp32/hal_impl.cpp`
  - `src/hal_esp32/sd_logger_esp32.cpp`
  - `src/hal_esp32/sd_ota_esp32.cpp`
  - `src/logic/runtime_config.cpp`
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h`
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h`
  - `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  - `docs/adr/subsystems/ADR-LOG-001-logging-buffering-and-sd-flush-policy.md`

- **files_write**:
  - `src/logic/modules.h` (`MOD_SD`)
  - `src/logic/modules.cpp` (Feature-Tabelle, Compile-/HW-/State-Logik)
  - `include/soft_config.h` (`MOD_DEFAULT_SD`)
  - `src/hal/hal.h` (HAL-Signatur für SD-Detect)
  - `src/hal_esp32/hal_impl.cpp` (SD-Detect-Implementierung)
  - `src/main.cpp` (Boot-Reihenfolge + SD-Gating)
  - `src/hal_esp32/sd_logger_esp32.cpp` (MOD_SD-Gating)
  - `src/hal_esp32/sd_ota_esp32.cpp` (MOD_SD-Gating)
  - `src/logic/runtime_config.cpp` (MOD_SD-Gating)
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h` (SD-Detect + FEAT_PINS_SD)
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h` (SD-Detect + FEAT_PINS_SD)
  - `backlog/index.yaml` (Task registrieren)

- **public_surface**:
  - `src/logic/modules.h` (neue öffentliche Feature-ID `MOD_SD`)
  - `src/hal/hal.h` (neue HAL-API für SD-Kartenpräsenz)
  - Board-Profile (`SD_DETECT_PIN`, `FEAT_PINS_SD`)

- **merge_risk_files**:
  - `src/main.cpp`
  - `src/logic/modules.cpp`
  - `src/hal_esp32/hal_impl.cpp`
  - `include/board_profile/*`

- **risk_notes**:
  - SD-Detect-Polaritität kann je Board unterschiedlich sein (active-low/high).
  - Boards ohne echten Detect-Pin brauchen explizites Fallback-Verhalten.
  - Erweiterung von `MOD_COUNT` kann Array- und Switch-Stellen brechen, wenn unvollständig.
  - Frühes Boot-Gating darf bestehende OTA-/Logging-Flows nicht regressieren.

- **Invarianten**:
  - Ohne erkannte SD-Karte dürfen SD-Funktionen nicht aktiv laufen.
  - Der schnelle Regelpfad bleibt frei von direkten SD-Zugriffen (ADR-LOG-001).
  - Modulzustände und Dependencies bleiben explizit und konfliktgeprüft (ADR-003).

- **Known traps**:
  - Versteckte SD-Reinit-Pfade in `sd_logger_esp32.cpp` und `sd_ota_esp32.cpp` müssen
    zusätzlich zum Startpfad gegated werden.
  - `runtime_config.cpp` kann SD-Zugriffe früh im Boot auslösen; Reihenfolge in `main.cpp`
    muss konsistent sein.

- **Rejected alternatives**:
  - Nur lokale `SD.begin()`-Fehlerbehandlung in jedem Pfad:
    - verworfen, weil kein zentraler Modulzustand und uneinheitliche Diagnostik.
  - Reines Compile-Time-Gating:
    - verworfen, weil die Anforderung explizit Runtime-Bootprüfung fordert.

- **AC**:
  - `FirmwareFeatureId` enthält `MOD_SD`; `MOD_COUNT` ist konsistent angepasst.
  - Feature-Tabelle enthält SD-Eintrag mit definierter `compiled/state/hw_detected`-Logik.
  - Beim Boot ohne SD-Karte: `moduleIsActive(MOD_SD) == false`.
  - Bei inaktivem `MOD_SD`:
    - kein SD-Logger-Start,
    - kein SD-OTA-Check/-Update,
    - kein SD-Credentials-Laden in `runtime_config`.
  - Beim Boot mit SD-Karte funktioniert bisherige SD-Funktionalität weiter.
  - Boot-Log enthält klare SD-Detect-Entscheidung.
  - Build erfolgreich für relevante Environments (`gnss_buildup`, `gnss_bringup_ntrip`).

- **verification**:
  - `pio run -e gnss_buildup`
  - `pio run -e gnss_bringup_ntrip`
  - `rg "MOD_SD|SD_DETECT_PIN|hal_sd_card_present" src include`
  - Hardwarevergleich mit SD gesteckt/gezogen (Boot-Log + Funktionsgating)

- **Links**:
  - `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  - `docs/adr/subsystems/ADR-LOG-001-logging-buffering-and-sd-flush-policy.md`
  - `backlog/tasks/TASK-027-modul-system-mit-runtime-aktivierung-und-pin-claim-arbitrierung.md`
