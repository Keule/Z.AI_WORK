# TASK-046 [#process-hygiene] Post-hoc Review und Backlog-Konservierung der Refactor-Welle

- **ID**: TASK-046
- **Titel**: [#process-hygiene] Post-hoc Review und Backlog-Konservierung der Refactor-Welle
- **Status**: open
- **Priorität**: high
- **Komponenten**: `backlog/tasks/*`, `backlog/index.yaml`, `reports/TASK-046/*`, `src/main.cpp`, `src/logic/cli.*`, `src/logic/nvs_config.*`, `src/logic/um980_uart_setup.*`, `platformio.ini`, `partitions_ota_spiffs_nvs_dump_factory.csv`
- **Dependencies**: none
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer
- **Epic**: EPIC-003

- **classification**: independent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Origin**:
  Nutzeranforderung aus laufender KI-Session (post-hoc):
  - Rolle: KI-Planer + KI-Reviewer
  - Auftrag: Nachträglichen Backlog-Task anlegen (für nicht abgedeckte Arbeit), Arbeitsbericht schreiben und die gesamte Arbeit seit Beginn der Refactor-Welle reviewen.

- **Diskussion**:
  Direkt: https://chat.z.ai/

- **Kontext/Problem**:
  Die Refactor-Welle (ModuleOps, Feature-Gates, Boot-Maintenance, CLI/NVS/UART/Web-OTA) wurde als großer Change-Block umgesetzt,
  jedoch ohne durchgehend nachvollziehbare post-hoc Prozesskonservierung für alle Zwischenänderungen.

  Es fehlen/fehlten insbesondere:
  1. konsolidierte Review-Perspektive über die gesamte Welle,
  2. eindeutige Zuordnung von Findings zu Nacharbeitsbedarf,
  3. stabiler Report-Artefaktpfad für spätere Agenten/Menschen ohne Chat-Kontext.

- **Pflichtlektüre vor Umsetzung**:
  1. `README.md`
  2. `agents.md`
  3. `backlog/README.md`
  4. `backlog/index.yaml`
  5. dieser Task (`TASK-046`)
  6. relevante ADRs zu Task-Modell/Runtime-Gating (mind. `docs/adr/README.md` und verlinkte ADRs)

- **Scope (in)**:
  - Post-hoc Erstellung eines dedizierten Prozess-/Review-Tasks mit Marker `[#process-hygiene]`.
  - Erstellung eines ausführlichen Reports unter `reports/TASK-046/` mit:
    - Gesamtreview der Refactor-Welle,
    - Findings (kritisch/hoch/mittel/niedrig),
    - Risiken, Regressionen, offene Punkte,
    - priorisierten Nacharbeitsvorschlägen.
  - Backlog-Index-Konsistenz herstellen (`backlog/index.yaml` + Task-Datei).

- **Nicht-Scope (out)**:
  - Direkte Implementierungsfixes in Firmware-Dateien.
  - ADR-Änderungen, sofern nicht explizit beauftragt.
  - Hardwarevalidierung selbst.

- **files_read**:
  - `README.md`
  - `agents.md`
  - `backlog/README.md`
  - `backlog/index.yaml`
  - `backlog/tasks/*` (relevante TASK-03x/04x)
  - `src/main.cpp`
  - `src/logic/cli.cpp`
  - `src/logic/nvs_config.*`
  - `src/logic/um980_uart_setup.*`
  - `platformio.ini`
  - `partitions_ota_spiffs_nvs_dump_factory.csv`

- **files_write**:
  - `backlog/tasks/TASK-046-process-hygiene-post-hoc-review-und-backlog-konservierung-refactor-welle.md`
  - `backlog/index.yaml`
  - `reports/TASK-046/gpt-5.3-codex.md`

- **public_surface**:
  - Backlog-Prozessartefakte
  - Review-/Report-Artefakte

- **merge_risk_files**:
  - `backlog/index.yaml`

- **Invarianten**:
  - Wichtige Erkenntnisse aus Chat/Review werden in Repo-Artefakten konserviert.
  - Backlog-Index und Task-Datei bleiben synchron.
  - Report ist ohne Chat-Kontext verständlich.

- **Known traps**:
  - Große Refactor-Commits verdecken kausale Zusammenhänge einzelner Regressionen.
  - Fehlende Toolverfügbarkeit (`pio`) erschwert harte Build-Validierung im Runtime-Container.

- **Rejected alternatives**:
  - Kein neuer Task, nur Chat-Zusammenfassung: verworfen (nicht dauerhaft konserviert).
  - Nur Kurzreport ohne Findings-Matrix: verworfen (zu wenig handlungsleitend).

- **AC**:
  - Neuer Task mit Marker `[#process-hygiene]` ist angelegt.
  - `backlog/index.yaml` enthält TASK-046 konsistent unter Epic + Task-Liste.
  - Report unter `reports/TASK-046/gpt-5.3-codex.md` liegt vor und enthält:
    - Scope des Reviews,
    - Findings-Matrix,
    - priorisierte Nacharbeit,
    - offene Risiken.
  - Review ist für Folgeagenten ohne Chat verständlich.

- **verification**:
  ```bash
  test -f backlog/tasks/TASK-046-process-hygiene-post-hoc-review-und-backlog-konservierung-refactor-welle.md
  rg -n "TASK-046" backlog/index.yaml backlog/tasks/TASK-046-process-hygiene-post-hoc-review-und-backlog-konservierung-refactor-welle.md
  test -f reports/TASK-046/gpt-5.3-codex.md
  ```

- **Links**:
  - `README.md`
  - `agents.md`
  - `backlog/README.md`
  - `backlog/index.yaml`
  - `templates/dev-report.md`
