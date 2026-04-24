# TASK-020 TFT_eSPI- und LilyGO-ETH-Lite-Treiber-Integration planen

- **ID**: TASK-020
- **Titel**: TFT_eSPI-Setup216 und LilyGO-T-ETH-Lite-ESP32 Treiber-Einbindung für reproduzierbare Builds planen
- **Status**: open
- **Priorität**: medium
- **Komponenten**: `platformio.ini`, `lib/`, `src/hal_esp32/*`, `docs/`, Build-/Dependency-Management
- **Dependencies**: TASK-013
- **Kontext/Problem**:
  - Im Projekt ist aktuell kein `lib/TFT_eSPI/` vorhanden; das gewünschte Setup `lib/TFT_eSPI/User_Setups/Setup216_LilyGo_ETH_Lite_ESP32.h` kann daher noch nicht genutzt werden.
  - Gleichzeitig existiert bereits ein aktiver Ethernet-Pfad über ESP-IDF/Arduino-ETH (`<ETH.h>`) inklusive Legacy-Fallback `ETHClass2`, wodurch Integrationskonflikte (SPI-Bus, Makros, doppelte Treiberinitialisierung) vermieden werden müssen.
  - Es fehlt ein verbindlicher Integrationsplan, wie TFT_eSPI und der in der LilyGO-Dokumentation referenzierte Treiber sauber eingebunden, versioniert und testbar gemacht werden.
- **Scope (in)**:
  - Integrationsentscheidung dokumentieren: Vendoring unter `lib/` vs. `lib_deps` (Version-Pinning, Update-Strategie, Reproduzierbarkeit).
  - Pin-/Bus-Konfliktanalyse für T-ETH-Lite-ESP32 (W5500 auf SPI3/HSPI, Display-Bus, CS/DC/RST/BL).
  - Aufgabenpakete für KI-Agenten definieren (Backlog-Subtasks mit klaren AC und Verifikationsschritten).
  - Zielbild für Board-spezifische Setup-Aktivierung (`User_Setup_Select.h`, Build-Flag-Strategie oder projektspezifisches Wrapper-Setup).
- **Nicht-Scope (out)**:
  - Vollständige Implementierung des TFT-UI-Stacks.
  - Feldvalidierung mit finalem HMI-Design.
- **AC**:
  - Ein dokumentierter Entscheid für die Einbindungsstrategie liegt vor (inkl. Vor-/Nachteile, Risiko, Rollback).
  - Ein reproduzierbarer Plan beschreibt, wie `Setup216_LilyGo_ETH_Lite_ESP32.h` im Projekt aktiviert wird, ohne den bestehenden ETH-Datenpfad zu brechen.
  - Mindestens drei umsetzbare Agenten-Aufgaben sind spezifiziert: Dependency-Setup, Compile-Validierung, Laufzeit-Smoke-Test.
  - Für jede Agenten-Aufgabe sind `files_read`, `files_write`, `risk_notes` und `verification` definiert.
- **Verifikation/Test**:
  - Plan-Review gegen bestehende Netzwerk- und Pin-Definitionen im Repo.
  - Build-Readiness-Check (mindestens Dry-Run-Compilepfad für betroffene Environments definiert).
- **Owner**: ki-planer
- **Links**:
  - `backlog/epics/EPIC-003-platform-and-reuse.md`
  - `include/hardware_pins.h`
  - `src/hal_esp32/hal_impl.cpp`
  - `platformio.ini`
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Entscheidungsnotiz**: `docs/board-selection-and-display-setup.md`


## Planer-Entscheidungsvorschlag (2026-04-17)

- Setup216 ist überwiegend eine Sammlung von `#define`-Konstanten; diese sollen **selektiv** in projekt-eigene Board-Profil-Header übernommen werden (kein ungepflegter Full-Fork der gesamten TFT_eSPI-Library).
- `hardware_pins.h` bleibt Quelle für allgemeine Board-/Bus-Pins (ETH, Sensorik). TFT/ETH-bezogene Defines werden getrennt in `include/board_profile/*` geführt, um Kollisionen und Kopplung gering zu halten.
- Kompilierbare Board-Auswahl erfolgt über bestehende PlatformIO-Environments und Board-Makros (`LILYGO_T_ETH_LITE_ESP32`, `LILYGO_T_ETH_LITE_ESP32S3`); Die Code-Auswahl erfolgt über bestehende Board-Makros und einen zentralen Header-Dispatcher (`board_profile_select.h`).
- Vor Implementierung ist eine Pin-/Bus-Verträglichkeitsprüfung Pflicht (insbesondere W5500 auf SPI3_HOST vs. TFT-Bus/CS), damit Ethernet-Pfad stabil bleibt.
