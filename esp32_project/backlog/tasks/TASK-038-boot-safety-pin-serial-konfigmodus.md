# TASK-038 Boot-Safety-Pin und deterministischer Serial-Konfigmodus

- **ID**: TASK-038
- **Titel**: Boot-Pfad analysieren und deterministischen Serial-Konfigmodus bei Safety-Pin LOW spezifizieren
- **Status**: open
- **Priorität**: high
- **Komponenten**: `src/main.cpp`, `src/logic/modules.*`, `src/logic/runtime_config.*`, `src/hal/hal.h`, `src/hal_esp32/hal_impl.cpp`, ggf. `src/hal_esp32/*serial*`
- **Dependencies**: TASK-037, TASK-022, TASK-023, TASK-028
- **delivery_mode**: mixed
- **task_category**: platform_reuse
- **Owner**: ki-planer
- **Epic**: EPIC-003

- **classification**: dependent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Origin**:
  Nutzeranforderung aus Chat (2026-04-21): Neuer Task für Boot-Analyse mit Fokus auf
  Safety-Pin, Boot-Sequenz und früheste serielle Verfügbarkeit; inklusive klarer
  Akzeptanzkriterien für deterministischen Serial-Konfigmodus.

- **Diskussion**:
  - Direkt: https://chatgpt.com/codex/cloud/tasks

- **Explizite Instruktion vor jeder Arbeit**:
  1. `README.md` lesen und einhalten.
  2. `agents.md` lesen und einhalten.
  3. Erst danach mit Analyse oder Implementierung beginnen.

- **Kontext/Problem**:
  Der aktuelle Boot-Pfad enthält mehrere Gating- und Initialisierungsschritte. Für
  Safety-/Bringup-Fälle fehlt eine eindeutig dokumentierte, deterministische Entscheidung,
  ob bei Safety-Pin LOW direkt ein Serial-Konfigmodus startet. Zusätzlich ist unklar,
  ab welchem frühesten Zeitpunkt die UART-Konsole verlässlich bedienbar ist.

- **Scope (in)**:
  - Boot-Pfad End-to-End analysieren (insb. Reihenfolge und Entscheidungen rund um:
    Safety-Pin-Readout, Modul-/Capability-Gating, Serial-Konsole, Auto-Boot).
  - Deterministische Eintrittsbedingung für Serial-Konfigmodus bei Safety-Pin LOW
    definieren und im Task/Code klar verankern.
  - Serial-Menü für Konfiguration auf UART-Konsole spezifizieren/umsetzen mit
    Mindestfunktionen: `Read`, `Show`, `Set`, `Save`, `Exit`.
  - Timeout-/Exit-Verhalten spezifizieren und dokumentieren (z. B. Auto-Boot nach X
    Sekunden ohne Eingabe).
  - Fail-safe Verhalten sicherstellen: ungültige Eingaben verursachen weder Crash noch
    undefinierte Zustände.
  - Dokumentieren, ab wann im Boot die serielle Konsole als „bedienbar“ gilt
    (früheste verlässliche Verfügbarkeit).

- **Nicht-Scope (out)**:
  - Keine Änderung an fachlichen PGN-Protokollen.
  - Kein Umbau des gesamten Task-Schedulers (`controlTask/commTask/maintTask`) ohne
    separaten Folge-Task/ADR.
  - Keine stillschweigende Änderung globaler Architekturregeln außerhalb des
    Boot-/Konfigpfads.

- **Pflichtlektüre vor Umsetzung**:
  1. `README.md`
  2. `agents.md`
  3. `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  4. `docs/adr/ADR-001-config-layering-fw-soft-runtime.md`
  5. `docs/adr/subsystems/ADR-BUILD-001-bringup-and-diagnostic-modes.md`
  6. `backlog/tasks/TASK-022-capabilities-compile-time-gating-und-onboarding-prompts.md`
  7. `backlog/tasks/TASK-023-capabilities-boot-init-gating-und-onboarding-prompts.md`
  8. `backlog/tasks/TASK-028-soft-config-mit-nutzer-defaults-und-runtime-konfiguration.md`
  9. dieser Task (`TASK-038`)

- **files_read**:
  - `src/main.cpp`
  - `src/logic/modules.h`
  - `src/logic/modules.cpp`
  - `src/logic/runtime_config.h`
  - `src/logic/runtime_config.cpp`
  - `src/hal/hal.h`
  - `src/hal_esp32/hal_impl.cpp`
  - relevante Serial-/Console-Dateien unter `src/hal_esp32/*` und `src/logic/*`
  - `include/soft_config.h`
  - `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  - `docs/adr/ADR-001-config-layering-fw-soft-runtime.md`
  - `docs/adr/subsystems/ADR-BUILD-001-bringup-and-diagnostic-modes.md`
  - `backlog/tasks/TASK-022-capabilities-compile-time-gating-und-onboarding-prompts.md`
  - `backlog/tasks/TASK-023-capabilities-boot-init-gating-und-onboarding-prompts.md`
  - `backlog/tasks/TASK-028-soft-config-mit-nutzer-defaults-und-runtime-konfiguration.md`

- **files_write**:
  - `backlog/tasks/TASK-038-boot-safety-pin-serial-konfigmodus.md`
  - `backlog/index.yaml`
  - ggf. betroffene Boot-/Serial-Dateien unter `src/main.cpp`, `src/logic/*`,
    `src/hal/*`, `src/hal_esp32/*` (nur falls Umsetzung im selben Task erfolgt)
  - ggf. ADR-Datei unter `docs/adr/` (falls Architekturentscheidung neu entsteht)

- **public_surface**:
  - Boot-Verhalten am Safety-Pin (extern beobachtbares Startverhalten).
  - UART-Serial-Konfigmenü (Operator-Interface inkl. Kommandos und Timeoutverhalten).
  - ggf. neue/erweiterte HAL- oder RuntimeConfig-Schnittstellen für Boot-/Config-Gating.

- **merge_risk_files**:
  - `src/main.cpp`
  - `src/logic/modules.cpp`
  - `src/logic/runtime_config.cpp`
  - `src/hal_esp32/hal_impl.cpp`
  - `include/soft_config.h`

- **Invarianten**:
  - Safety-Pin LOW beim Boot führt deterministisch in den Serial-Konfigmodus.
  - Ungültige UART-Eingaben dürfen keine Crashes oder undefinierten Zustände erzeugen.
  - Timeout/Exit-Verhalten ist eindeutig dokumentiert und reproduzierbar.
  - Bestehende Modul-/Capability-Gating-Regeln aus TASK-022/TASK-023 bleiben konsistent.
  - Konfig-Layering gemäß ADR-001 bleibt erhalten (keine verdeckte Umgehung von
    `soft_config`/`runtime_config`).

- **known traps**:
  - Race Conditions durch zu frühes Lesen des Safety-Pins vor stabiler GPIO-Initialisierung.
  - Serial-Konsole „verfügbar“, aber noch nicht vollständig initialisiert (falsche
    Annahmen über frühesten Interaktionszeitpunkt).
  - Blockierende Menüloops können Auto-Boot oder Watchdog-Verhalten unbeabsichtigt
    beeinflussen.
  - Inkonsistente `Save`-Semantik (RAM vs. persistiert) kann zu Fehlbedienung führen.
  - Boot-Gating-Änderungen ohne klare Architekturentscheidung erzeugen Drift zwischen
    Dokumentation und Ist-Code.

- **Rejected alternatives**:
  - Safety-Pin nur als „best effort“-Hinweis ohne deterministisches Verhalten:
    verworfen, da Sicherheits-/Servicepfad reproduzierbar sein muss.
  - Nur implizite UART-CLI ohne dokumentiertes Timeout/Exit:
    verworfen, da Feldbetrieb eindeutige Rückfalllogik benötigt.

- **AC**:
  - Wenn Safety-Pin beim Boot LOW ist, startet deterministisch der Serial-Konfigmodus.
  - Das Menü ist über UART-Konsole bedienbar und umfasst mindestens:
    `Read`, `Show`, `Set`, `Save`, `Exit`.
  - Timeout-/Exit-Verhalten ist dokumentiert (z. B. Auto-Boot nach X Sekunden ohne
    Eingabe) und im Verhalten nachvollziehbar.
  - Fail-safe: Bei ungültiger Eingabe kein Crash, keine undefinierten Zustände.
  - Abhängigkeiten zu TASK-022, TASK-023, TASK-028 sind in Planung/Umsetzung
    berücksichtigt und im Report explizit geprüft.
  - Boot-Gating-Architektur ist entweder durch bestehenden ADR-Verweis abgedeckt
    (`ADR-BUILD-001`) oder ein ADR-Bedarf ist explizit markiert und nachverfolgbar.

- **verification**:
  - `pio run` (mindestens relevantes Zielprofil)
  - Log-/UART-Session mit Safety-Pin LOW/HIGH beim Boot vergleichen
  - Negativtests mit ungültigen Menüeingaben (keine Crashes, definierte Fehlermeldungen)
  - Review-Check: Doku von Timeout/Exit und frühester serieller Verfügbarkeit vorhanden

- **ADR-/Architekturhinweis (Boot-Gating)**:
  - Primärreferenz: `docs/adr/subsystems/ADR-BUILD-001-bringup-and-diagnostic-modes.md`
  - Falls produktiver Standard-Bootpfad (nicht nur Diagnose-Sonderpfad) semantisch
    geändert wird und ADR-BUILD-001 die Entscheidung nicht ausreichend abdeckt:
    **ADR-Bedarf explizit markieren und neue ADR anlegen**.

- **Links**:
  - `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  - `docs/adr/ADR-001-config-layering-fw-soft-runtime.md`
  - `docs/adr/subsystems/ADR-BUILD-001-bringup-and-diagnostic-modes.md`
  - `backlog/tasks/TASK-022-capabilities-compile-time-gating-und-onboarding-prompts.md`
  - `backlog/tasks/TASK-023-capabilities-boot-init-gating-und-onboarding-prompts.md`
  - `backlog/tasks/TASK-028-soft-config-mit-nutzer-defaults-und-runtime-konfiguration.md`
