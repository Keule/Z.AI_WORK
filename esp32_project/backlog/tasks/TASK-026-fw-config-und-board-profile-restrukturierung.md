# TASK-026 fw_config und Board-Profil restrukturierung

- **ID**: TASK-026
- **Titel**: `fw_config` und Board-Profile restrukturieren — `hardware_pins.h` aufteilen
- **Status**: done
- **Priorität**: medium
- **Komponenten**: include/hardware_pins.h, include/fw_config.h, include/board_profile/*, src/hal_esp32/hal_impl.cpp
- **Dependencies**: none
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer
- **Epic**: EPIC-003

- **classification**: independent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Origin**:
  Nutzer-Anforderung: "`hardware_pins.h` sollte umbenannt werden in sowas wie `config` oder `global_config`, da ist viel mehr ihre Funktion (auch mit Firmware-Version, etc.)". Ergebnis: Aufteilung in `fw_config.h` (Build-Time Board+Firmware Config) und `soft_config.h` (Nutzer-Defaults, Runtime editierbar). Board-Profile bleiben reine Pin-Dateien.

- **Diskussion**:
  - Direkt: https://chat.z.ai/c/d6f6eb9b-9217-401b-bb23-08e8c0fbca69
  - Shared: https://chat.z.ai/s/a858dd17-02e3-416c-a123-649830256a4e

- **Kontext/Problem**:
  `include/hardware_pins.h` ist ein Thin-Wrapper um `board_profile_select.h` mit 3 OTA-`#define`s, die nichts mit Pins zu tun haben. Der Name suggeriert "nur Pins", aber die Datei wird als zentraler Entry-Point für alle Board-Konfiguration genutzt. Eine saubere Trennung fehlt. Board-Profile enthalten zudem veraltete Kommentare (Pin-Belegungen die nicht mehr stimmen), die bereinigt werden sollten.

- **Scope (in)**:
  - `include/hardware_pins.h` → `include/fw_config.h` umbenennen.
  - `fw_config.h`: Enthält `#include "board_profile/board_profile_select.h"`, OTA-Pfade (`SD_FW_FILE_*`), Firmware-Versions-Defines.
  - Board-Profile (`include/board_profile/*_board_pins.h`): Bereinigung — nur Pin-Defines behalten, veraltete SPI-Bus-Kommentare aktualisieren oder entfernen.
  - Alle `#include "hardware_pins.h"` im Codebase durch `#include "fw_config.h"` ersetzen.
  - `hal_impl.cpp` Header-Kommentare mit falschen Pin-Belegungen korrigieren.
  - Veraltete Kopien in `include/LILYGO_T_ETH_LITE_ESP32*_board_pins.h` (top-level) löschen oder als Deprecated markieren.

- **Nicht-Scope (out)**:
  - `soft_config.h` erstellen (folgt in TASK-028).
  - `FEAT_PINS_*` Pin-Gruppen pro Feature definieren (folgt in TASK-027).
  - Logikänderungen an Modul-System oder Init-Pfaden.

- **files_read**:
  - `include/hardware_pins.h`
  - `include/board_profile/board_profile_select.h`
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h`
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h`
  - `src/hal_esp32/hal_impl.cpp`
  - Alle Dateien, die `#include "hardware_pins.h"` enthalten

- **files_write**:
  - `include/fw_config.h` (NEU — umbenannt + erweitert aus hardware_pins.h)
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h` (Kommentare bereinigt)
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h` (Kommentare bereinigt)
  - `src/hal_esp32/hal_impl.cpp` (Include + Header-Kommentare korrigiert)
  - Alle Dateien mit `#include "hardware_pins.h"` → `#include "fw_config.h"`
  - `include/hardware_pins.h` → LOESCHEN (oder Redirect-Wrapper mit Deprecated-Warnung)

- **public_surface**:
  - `include/fw_config.h` — neuer zentraler Include-Punkt
  - `include/board_profile/*_board_pins.h` — Pin-Defines bleiben oeffentlich

- **merge_risk_files**:
  - `include/hardware_pins.h` / `include/fw_config.h` — von vielen Dateien inkludiert
  - `src/hal_esp32/hal_impl.cpp` — grosse Datei, wird von mehreren Tasks beruehrt

- **risk_notes**:
  - Umbenennen des Include erfordert Suchen/Ersetzen in allen Dateien — Risiko, eine Stelle zu uebersehen. Build-Check nach Aenderung zwingend.
  - Veraltete Kopien der Board-Profile im Top-Level `include/` koennten zu Verwirrung fuehren, wenn sie nicht explizit behandelt werden.

- **AC**:
  - `include/fw_config.h` existiert, enthaelt OTA-Pfade und `#include "board_profile/board_profile_select.h"`.
  - `include/hardware_pins.h` existiert nicht mehr (oder ist leerer Deprecated-Redirect).
  - Alle `#include "hardware_pins.h"` Referenzen im Codebase sind durch `#include "fw_config.h"` ersetzt.
  - `pio run` baut fehlerfrei fuer mindestens `gnss_buildup` und `gnss_bringup_ntrip`.
  - Board-Profile enthalten keine veralteten Pin-Belegungen in Kommentaren mehr.
  - `hal_impl.cpp` Header-Kommentar spiegelt aktuelle Pin-Belegung des Board-Profils.

- **verification**:
  - `pio run` fuer alle relevanten Environments.
  - `grep -r "hardware_pins" src/ include/` — keine Treffer (oder nur Deprecated-Redirect).
  - Visueller Review der Board-Profile auf veraltete Kommentare.

- **Links**:
  - `backlog/epics/EPIC-003-platform-and-reuse.md`
  - `backlog/tasks/TASK-024-pin-claims-und-zuweisung-pro-init-pfad.md`
