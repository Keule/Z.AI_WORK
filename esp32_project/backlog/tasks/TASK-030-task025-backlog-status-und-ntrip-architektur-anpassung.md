# TASK-030 TASK-025 Backlog-Status und NTRIP-Architektur-Anpassung

- **ID**: TASK-030
- **Titel**: TASK-025 NTRIP auf neues Modul-System und maintTask migrieren
- **Status**: done
- **Priorität**: medium
- **Komponenten**: src/logic/ntrip.cpp, src/logic/ntrip.h, src/main.cpp, src/logic/modules.cpp, src/logic/global_state.h, src/logic/hw_status.cpp, backlog/index.yaml
- **Dependencies**: TASK-027, TASK-029
- **delivery_mode**: firmware_only
- **task_category**: feature_expansion
- **Owner**: ki-planer
- **Epic**: EPIC-004

- **classification**: dependent
- **exclusive_before**: [TASK-027, TASK-029]
- **parallelizable_after**: []

- **Origin**:
  Aus der Diskussion: NTRIP ist aktuell in commTask integriert mit blocking TCP-Connect (Problem). NTRIP ist Dead-Code wenn moduleActivate("NTRIP") nicht aufgerufen wird. NTRIP fehlt in hwStatusUpdate(). NTRIP-Konfiguration ist hardcoded. TASK-025 ist im Backlog als "open" gefuehrt, aber bereits implementiert — muss konsolidiert werden.

- **Diskussion**:
  - Direkt: https://chat.z.ai/c/d6f6eb9b-9217-401b-bb23-08e8c0fbca69
  - Shared: https://chat.z.ai/s/a858dd17-02e3-416c-a123-649830256a4e

- **Kontext/Problem**:
  TASK-025 (NTRIP-Client) ist implementiert, aber:
  1. Task-Status im Backlog ist "open" obwohl Code vorhanden.
  2. NTRIP-Connect ist blocking in commTask (wird durch TASK-029 geloest).
  3. NTRIP-Konfiguration ist hardcoded in main.cpp (wird durch TASK-028 geloest).
  4. NTRIP fehlt in `hwStatusUpdate()` — Subsystem-Status wird nicht gemeldet.
  5. NTRIP nutzt das alte `AogModuleInfo` System (wird durch TASK-027 ersetzt).
  6. Kein Entwickler-Report fuer TASK-025 vorhanden.
  Dieser Task konsolidiert NTRIP mit den neuen Architektur-Komponenten aus TASK-027/028/029.

- **Scope (in)**:
  - **TASK-025 Status**: Auf `done` setzen im Backlog (inkl. index.yaml).
  - **NTRIP im Modul-System**: NTRIP wird ueber `moduleActivate("NTRIP")` gestartet (nach TASK-027). Pin-Claim nicht noetig (eigene Pins = keine). Abhaengigkeit: ETH muss aktiv sein.
  - **NTRIP Config aus soft_config**: Host/Port/Mountpoint/User/Pass aus `cfg::` Defaults (nach TASK-028).
  - **NTRIP Connect in maintTask**: Durch TASK-029 bereits geloest — dieser Task stellt sicher, dass die Integration sauber ist.
  - **hwStatusUpdate()**: NTRIP-Status (connected/disconnected/error) wird in `hwStatusUpdate()` beruecksichtigt.
  - **NTRIP Feature-Gating vereinheitlichen**: `#if FEAT_ENABLED(FEAT_NTRIP)` um alle NTRIP-Aufrufe in commTask, maintTask und modules.cpp.
  - **Entwickler-Report**: Nachhol-Report fuer TASK-025 (inkl. Build-Fixes aus vorheriger Session: snprintf, LOG_LEVEL_NTRIP, uint8_t overflow).

- **Nicht-Scope (out)**:
  - NTRIP-Logik selbst aendern (bereits implementiert).
  - Neue NTRIP-Funktionen hinzufuegen.

- **files_read**:
  - `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md`
  - `backlog/index.yaml`
  - `src/logic/ntrip.cpp`
  - `src/logic/ntrip.h`
  - `src/main.cpp`
  - `src/logic/modules.cpp` (nach TASK-027)
  - `src/logic/hw_status.cpp`
  - `include/soft_config.h` (nach TASK-028)

- **files_write**:
  - `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md` (Status → done)
  - `backlog/index.yaml` (TASK-025 status → done)
  - `src/logic/modules.cpp` (NTRIP im Modul-System, moduleActivate-Aufruf)
  - `src/logic/hw_status.cpp` (NTRIP-Status in hwStatusUpdate)
  - `src/main.cpp` (NTRIP-Config aus cfg::, NTRIP im maintTask)
  - `reports/TASK-025/` (Nachhol-Entwickler-Report)

- **public_surface**:
  - `backlog/index.yaml` — Task-Status-Aenderung
  - `src/logic/hw_status.cpp` — erweitertes Monitoring

- **merge_risk_files**:
  - `src/main.cpp` — wird von TASK-027, TASK-029 und diesem Task geaendert → exklusiver Lock nach 027/029 Merge
  - `backlog/index.yaml` — Backlog-Index

- **risk_notes**:
  - TASK-030 muss warten bis TASK-027 und TASK-029 gemerged sind. Parallele Arbeit an main.cpp wuerde Konflikte erzeugen.
  - Nachhol-Report fuer TASK-025 muss die Build-Fixes aus vorheriger Session dokumentieren (snprintf, LOG_LEVEL_NTRIP, uint8_t overflow).

- **AC**:
  - TASK-025 Status im Backlog und index.yaml = `done`.
  - NTRIP wird ueber `moduleActivate("NTRIP")` gestartet.
  - NTRIP-Config (Host, Port, Mountpoint, User, Pass) kommt aus `cfg::` Defaults in `soft_config.h`.
  - NTRIP-Connect laeuft in maintTask (non-blocking in commTask).
  - `hwStatusUpdate()` beruecksichtigt NTRIP-Verbindungsstatus.
  - `#if FEAT_ENABLED(FEAT_NTRIP)` Guards konsistent um alle NTRIP-Aufrufe.
  - Nachhol-Entwickler-Report fuer TASK-025 unter `reports/TASK-025/` existiert.
  - `pio run` baut fehlerfrei.

- **verification**:
  - `pio run` fuer alle relevanten Environments.
  - Build mit `FEAT_NTRIP` deaktiviert → kein NTRIP-Code im Binary.
  - Build mit `FEAT_NTRIP` aktiviert → NTRIP wird ueber Modul-System gestartet.

- **Links**:
  - `backlog/epics/EPIC-004-feature-expansion.md`
  - `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md`
  - `backlog/tasks/TASK-027-modul-system-mit-runtime-aktivierung-und-pin-claim-arbitrierung.md`
  - `backlog/tasks/TASK-028-soft-config-mit-nutzer-defaults-und-runtime-konfiguration.md`
  - `backlog/tasks/TASK-029-maintask-fuer-blocking-ops-und-psram-sd-logging.md`
