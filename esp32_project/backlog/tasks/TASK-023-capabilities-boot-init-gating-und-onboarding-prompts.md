# TASK-023 Boot-Initialisierung zusätzlicher Capabilities bedarfsgeführt steuern + Onboarding-Compliance im Promptpfad sichern

- **ID**: TASK-023
- **Titel**: Zusätzliche SPI/UART-Capabilities nur bei Modulbedarf initialisieren und Onboarding-Compliance im Codex-Task-Promptpfad verifizieren
- **Status**: done
- **Priorität**: high
- **Komponenten**: Boot-Init-Pfad, HAL-Initialisierung, Capability-Registry/Feature-Abfrage, Prompt-Pipeline-Compliance
- **Dependencies**: TASK-022
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer

- **classification**: dependent
- **exclusive_before**:
  - TASK-022
- **parallelizable_after**: []

- **Kontext/Problem**:
  - Compile-Time-Gating allein reicht nicht aus: Laufzeitseitig müssen zusätzliche SPI/UART-Capabilities beim Boot nur dann initialisiert werden, wenn die aktivierten Module sie tatsächlich benötigen.
  - Nach der Prompt-Erweiterung aus TASK-022 muss sichergestellt bleiben, dass dieser Onboarding-Block auch im effektiven Übergabe-Flow nicht verloren geht.

- **Scope (in)**:
  - Boot-Initialisierung so umstellen, dass Initialisierungspfade bedarfsgeführt (modulgetrieben) entscheiden.
  - Kein Init für inaktive/ungenutzte Zusatz-Capabilities.
  - Robustheit: saubere Logs/Fallbacks bei nicht benötigten oder nicht verfügbaren Capabilities.
  - Promptpfad-Compliance prüfen (Button -> erzeugter Prompttext), Onboarding Teil 1 weiterhin enthalten.

- **Nicht-Scope (out)**:
  - Neudesign des gesamten HAL-Layers.
  - Endgültige Pin-Claim-Konfliktauflösung pro Init-Pfad (folgt in TASK-024).

- **files_read**:
  - `src/main.cpp`
  - `src/hal_esp32/hal_impl.cpp`
  - weitere Initialisierungseinheiten unter `src/hal_esp32/*`
  - `src/logic/features.h`
  - Prompt-Quelle/Generator der klickbaren Codex-Task-Buttons
  - `README.md`, `docs/process/PLAN_AGENT.md`, `docs/process/QUICKSTART_WORKFLOW.md`

- **files_write**:
  - `src/main.cpp`
  - `src/hal_esp32/hal_impl.cpp`
  - ggf. weitere HAL-/Init-Dateien unter `src/hal_esp32/*`
  - ggf. Promptpfad-Datei(en), falls Onboarding-Block in der tatsächlichen Ausgabe verloren geht

- **risk_notes**:
  - Falsche Reihenfolge im Boot kann abhängige Module trotz korrekter Compile-Time-Flags unbrauchbar machen.
  - Verdeckte Seiteneffekte durch bisher implizite Init-Aufrufe.
  - Prompt-Compliance kann durch Zwischenformatierung (z. B. Serializer/Template-Wrapper) unbemerkt regressieren.

- **AC**:
  - Zusätzliche SPI/UART-Capabilities werden nur initialisiert, wenn ein zugeordnetes Modul aktiv ist.
  - Bei deaktiviertem Modul erfolgt kein entsprechender Init-Aufruf (durch Logs/Instrumentierung nachvollziehbar).
  - Bootpfad bleibt für bestehende Standardmodule funktionsgleich (keine Regression der bisherigen Minimalinitialisierung).
  - Der effektive, aus dem Button erzeugte Codex-Prompt enthält weiterhin den Pflicht-Onboarding-Block Teil 1.

- **verification**:
  - Build-Check: `pio run`
  - Laufzeit-/Logcheck gemäß bestehendem Diagnosesetup (gezielte Prüfung: Init-Aufruf vorhanden/nicht vorhanden je Modulkonfiguration)
  - Stichprobe des final erzeugten Prompttexts auf die drei Pflichtdokumente

- **Links**:
  - `backlog/tasks/TASK-022-capabilities-compile-time-gating-und-onboarding-prompts.md`
  - `src/main.cpp`
  - `src/hal_esp32/hal_impl.cpp`
