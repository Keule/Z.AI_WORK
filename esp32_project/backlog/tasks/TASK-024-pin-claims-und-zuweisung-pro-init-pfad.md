# TASK-024 Konfliktarme Pin-Claims und Pin-Zuweisung je Initialisierungspfad festziehen

- **ID**: TASK-024
- **Titel**: Pin-Claims und Pin-Zuweisung pro Capability-Initialisierungspfad verbindlich und konfliktarm umsetzen
- **Status**: done
- **Priorität**: high
- **Komponenten**: Hardware-Pin-Definitionen, HAL-Init-Pfade, Capability-Routing, Konfliktprüfung
- **Dependencies**: TASK-023
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer

- **classification**: dependent
- **exclusive_before**:
  - TASK-023
- **parallelizable_after**: []

- **Kontext/Problem**:
  - Nach Compile-Time- und Boot-Gating müssen Pins entlang der tatsächlichen Initialisierungspfade eindeutig zugewiesen werden.
  - Ohne klare Pin-Claims entstehen Konflikte (doppelte Nutzung, falsche Buszuordnung, Boot-Strap-Kollisionen), insbesondere bei optional aktivierten Zusatz-UART/SPI-Pfaden.

- **Scope (in)**:
  - Pro Initialisierungspfad klare Pin-Claim-Regeln und Pin-Zuweisung definieren/implementieren.
  - Konfliktfälle explizit erkennen und deterministisch behandeln (Fehlerlog, Fallback, Abbruchpfad).
  - Konsistente Verbindung zu den Modul-/Capability-Entscheidungen aus TASK-022/023 sicherstellen.
  - Prüfen, dass die in TASK-022 eingeführten Pflicht-Onboarding-Hinweise in den KI-Entwickler-Prompts unverändert bestehen (Regression Guard).

- **Nicht-Scope (out)**:
  - Neue Hardwarefeatures außerhalb der bereits geplanten Zusatz-SPI/UART-Capabilities.
  - Erweiterung des Pin-Systems für völlig neue Boards ohne direkten Bezug zu diesem Capability-Paket.

- **files_read**:
  - `include/hardware_pins.h`
  - `src/hal_esp32/hal_impl.cpp`
  - ggf. weitere Pin-/Boardprofile unter `include/board_profile/*`
  - `src/main.cpp`
  - Task-Promptquelle (klickbare Codex-Buttons) für Onboarding-Regression-Check

- **files_write**:
  - `include/hardware_pins.h`
  - `src/hal_esp32/hal_impl.cpp`
  - ggf. board-spezifische Profilheader unter `include/board_profile/*`
  - ggf. ergänzende Doku zur Pin-Claim-Logik

- **risk_notes**:
  - Falsche oder mehrdeutige Pin-Claims können Bootfehler oder instabile Peripherie verursachen.
  - Board-spezifische Unterschiede (ESP32 vs. ESP32-S3) können zu stillen Konflikten führen.
  - Unklare Fallback-Strategien erschweren Fehlersuche im Feld.

- **AC**:
  - Für jeden relevanten Init-Pfad (mind. ein Zusatz-SPI + ein Zusatz-UART) ist eine eindeutige Pin-Zuweisung dokumentiert und im Code nachvollziehbar.
  - Kollisionen werden erkannt und führen zu klarer, reproduzierbarer Behandlung (nicht stillschweigend überschrieben).
  - Keine doppelte Pin-Belegung in den unterstützten Standardprofilen laut statischer Prüfung/Review.
  - Onboarding-Pflichtblock in den Codex-Task-Prompts bleibt vorhanden (kein Regressionsverlust).

- **verification**:
  - Build-Check: `pio run`
  - Statischer Pin-Claim-Review gegen `include/hardware_pins.h` und betroffene Init-Pfade
  - Prüfen der Promptausgabe auf unveränderte Onboarding-Referenzen

- **Links**:
  - `backlog/tasks/TASK-023-capabilities-boot-init-gating-und-onboarding-prompts.md`
  - `include/hardware_pins.h`
  - `src/hal_esp32/hal_impl.cpp`
