# TASK-036 – Entwicklerreport (codex)

## Umsetzung
- `MOD_SD` als neues Feature-Modul ergänzt (`FirmwareFeatureId`, `MOD_COUNT`, Feature-Tabelle, Owner-Tag, Default-State über `cfg::MOD_DEFAULT_SD`).
- Boot-Detect-Gating umgesetzt:
  - Neue HAL-API `hal_sd_card_present()`.
  - SD-Präsenz in `modulesInit()` erfasst und in `ModuleHwStatus::sd_present` + `g_features[MOD_SD].hw_detected` gespiegelt.
  - Aktivierung in `setup()` nur bei erkanntem SD-Medium; sonst explizite Deaktivierung + Log.
- Hartes Gating über `moduleIsActive(MOD_SD)` umgesetzt für:
  - SD-Logger (Record-Pfad + Task-Init + Laufzeit-Loop).
  - SD-OTA (Availability-Check + Update-Entry).
  - SD-runtime_config-Overrides (`softConfigLoadOverrides`).
- Board-Profile auf „kein SD-Detect-Pin verfügbar“ korrigiert:
  - `FEAT_PINS_SD` nur mit `SD_CS`.
  - SD-Erkennung erfolgt stattdessen über einmalige SD-Init-Probe in HAL.
- Boot-Reihenfolge in `main.cpp` angepasst:
  - `modulesInit()` + Modulaktivierung (inkl. SD-Gating) vor SD-OTA/runtime_config-SD-Zugriffen.

## Verifikation / Checks
- `pio run -e gnss_buildup`
- `pio run -e gnss_bringup_ntrip`
- `rg "MOD_SD|hal_sd_card_present|moduleIsActive\(MOD_SD\)|FEAT_PINS_SD" src include`

## Hinweise / Risiken
- SD-Erkennung läuft nun über eine einmalige SD-Mount-Probe beim Boot. Bei Fehlschlag bleibt `MOD_SD` aus und alle SD-Pfade sind hart gegated.
- Die Probe gibt den SD/SPI-Kontext wieder frei (`SD.end()`, `sdSPI.end()`, `hal_sensor_spi_reinit()`), bevor der Rest des Systems weiterläuft.

## Offene Punkte
- Hardware-Validierung mit SD gesteckt/gezogen (Boot-Logs + OTA/Logger/RuntimeConfig-Gating) steht noch aus.
