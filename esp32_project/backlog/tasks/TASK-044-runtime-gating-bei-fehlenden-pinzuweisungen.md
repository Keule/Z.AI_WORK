# TASK-044 Runtime-Gating für nicht verfügbare Sensor-/Aktor-Module bei fehlenden Pinzuweisungen

- **ID**: TASK-044
- **Titel**: Control-/Init-Pfade konsequent über Modulverfügbarkeit gate'n, um Phantom-Nutzung ohne Pins zu verhindern
- **Status**: done
- **Priorität**: high
- **Komponenten**: `src/main.cpp`, `src/logic/control.cpp`, `src/logic/modules.cpp`, `src/hal_esp32/hal_impl.cpp`, `src/logic/hw_status.cpp`, `src/logic/global_state.*`
- **Dependencies**: TASK-027, TASK-036
- **delivery_mode**: firmware_only
- **task_category**: runtime_stability
- **Owner**: ki-planer
- **Epic**: EPIC-001

- **classification**: dependent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Origin**:
  Nutzeranforderung aus Chat (2026-04-22):
  - Prüfen, ob Features trotz fehlender Pinzuweisungen zur Laufzeit weiter initialisiert/benutzt werden.
  - Verdacht auf Watchdog-Auslöser insbesondere im IMU-Pfad.
  - Gewünschtes Ergebnis: klare technische Maßnahme als umsetzbarer Task.

- **Diskussion**:
  - Chat-Session in dieser Codex-Aufgabe (kein separater öffentlicher URL-Export verfügbar).

- **Kontext/Problem**:
  Das HAL-Boot-Init gate't IMU/ADS/ACT bereits mit `feat::*` und `FEAT_PINS_*_COUNT > 0`,
  wodurch fehlende Hardware nicht initialisiert wird. Im Control-Laufpfad werden jedoch
  Sensor-/Aktor-Zugriffe derzeit nicht durch `moduleIsActive(MOD_*)` abgesichert.
  Dadurch entstehen bei Boards/Profilen ohne entsprechende Pins unnötige Laufzeitzugriffe
  (Phantom-Nutzung), Diagnose-Rauschen und potenzielle Timing-/Stabilitätsrisiken.

- **Scope (in)**:
  - `controlStep()` so umbauen, dass IMU/ADS/ACT nur bei aktiven Modulen gelesen/geschrieben werden.
  - Für inaktive Module deterministische Fallback-Werte und Qualitätsflags setzen.
  - Task-Startbedingungen in `main.cpp` schärfen:
    - `controlTask` nur starten, wenn die benötigte Pipeline aktivierbar/aktiv ist.
    - klare Boot-Logs ausgeben, warum eine Pipeline deaktiviert bleibt.
  - Einheitliche Hilfsfunktion für Pipeline-Readiness einführen (z. B. in `modules.cpp`/`modules.h`).
  - `hw_status`-Meldungen so anpassen, dass „nicht aktiv“ von „Fehler“ unterscheidbar bleibt.
  - Dokumentierte Zuordnung „AOG Heartbeat-Watchdog vs. ESP Task-WDT“ in Logs/Kommentar absichern.

- **Nicht-Scope (out)**:
  - Keine Änderung am PGN-Protokoll oder Netzwerk-Frameformat.
  - Keine neuen Boardprofile/Pinbelegungen.
  - Kein Umbau des gesamten Feature-Flag-Systems.

- **Pflichtlektüre vor Umsetzung**:
  1. `README.md`
  2. `agents.md`
  3. `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  4. `docs/adr/subsystems/ADR-HAL-001-pin-conflict-handling-policy.md`
  5. dieser Task (`TASK-044`)

- **files_read**:
  - `src/main.cpp`
  - `src/logic/control.cpp`
  - `src/logic/modules.h`
  - `src/logic/modules.cpp`
  - `src/hal_esp32/hal_impl.cpp`
  - `src/logic/hw_status.cpp`
  - `src/logic/net.cpp`
  - `src/logic/dependency_policy.h`
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h`
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h`

- **files_write**:
  - `src/main.cpp`
  - `src/logic/control.cpp`
  - `src/logic/modules.h`
  - `src/logic/modules.cpp`
  - `src/logic/hw_status.cpp` (falls nötig für Status-Differenzierung)
  - `backlog/index.yaml`
  - `reports/TASK-044/<dev-report>.md`

- **public_surface**:
  - `src/logic/modules.h` (falls neue Pipeline-Readiness API)
  - `src/main.cpp` (Task-Startverhalten)
  - `src/logic/control.cpp` (Runtime-Gating-Semantik)

- **merge_risk_files**:
  - `src/main.cpp`
  - `src/logic/control.cpp`
  - `src/logic/modules.cpp`

- **Invarianten**:
  - Ein Modul ohne Pins/ohne erfolgreiche Aktivierung darf nicht als aktiv genutzt werden.
  - Harte Konfliktpolitik aus ADR-HAL-001 bleibt unverändert.
  - Compile-Time-Verfügbarkeit und Runtime-Aktivität bleiben getrennt (ADR-003).
  - Bei deaktivierter Pipeline muss das Verhalten deterministisch und sicher sein (kein stilles Weiterlaufen im Vollmodus).

- **Known traps**:
  - `feat::control()` allein ist kein Nachweis für aktive Hardware-Pipeline.
  - Eine pauschale `imuUpdate()` ohne Modul-Gate erzeugt irreführende „IMU unavailable“-Folgesymptome.
  - Heartbeat-Watchdog (PGN 254) und ESP Task-WDT dürfen diagnostisch nicht verwechselt werden.

- **Rejected alternatives**:
  - Nur zusätzliche Warnlogs ohne funktionales Gating:
    - verworfen, da das Laufzeitverhalten weiter inkonsistent bleibt.
  - Nur Build-Profile einschränken:
    - verworfen, da Laufzeit-Modulzustand und Board-Varianten weiterhin abgesichert werden müssen.

- **AC**:
  - `controlStep()` greift nur auf IMU/ADS/ACT zu, wenn jeweiliges Modul aktiv ist.
  - Bei inaktiven Modulen sind Status-/Qualitätsflags konsistent auf „nicht verfügbar“, ohne Endlosschleifen/Busy-Fail.
  - `controlTask` startet nicht in einem Modus, in dem die Mindestpipeline nicht aktiv ist.
  - Boot-Log zeigt eindeutig, welche Pipeline aktiv/deaktiviert wurde und warum.
  - Keine Regression für Profile mit vollständiger Sensor-/Aktor-Hardware.

- **verification**:
  - `pio run -e T-ETH-Lite-ESP32`
  - `pio run -e T-ETH-Lite-ESP32S3`
  - `pio run -e profile_comm_only`
  - `pio run -e profile_sensor_front`
  - `rg "moduleIsActive\(MOD_IMU\)|moduleIsActive\(MOD_ADS\)|moduleIsActive\(MOD_ACT\)" src`

- **Links**:
  - `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  - `docs/adr/subsystems/ADR-HAL-001-pin-conflict-handling-policy.md`
  - `backlog/tasks/TASK-027-modul-system-mit-runtime-aktivierung-und-pin-claim-arbitrierung.md`
  - `backlog/tasks/TASK-036-sd-modul-boot-detect-und-hartes-gating.md`
