# TASK-019 Integrationsplanung für zwei UM980-Module

- **ID**: TASK-019
- **Titel**: Integrationsplanung für zwei UM980-Module erstellen
- **Status**: in_progress
- **Priorität**: high
- **Komponenten**: `docs/plans/`, `src/hal_esp32/`, `src/logic/`, UART-/RTCM-Datenpfad, Konfigurationsmanagement
- **Dependencies**: TASK-013, TASK-014A, TASK-015, TASK-017
- **AC**:
  - Ein umsetzbarer Integrationsplan für den Parallelbetrieb von zwei UM980 liegt als Dokument vor (Topologie, Rollenverteilung, Datenflüsse, Fallback-Verhalten).
  - Die Schnittstellen zwischen HAL, Netzwerk und PGN-Pipeline sind mit klaren Verantwortlichkeiten, Änderungsbedarf und Risiken beschrieben.
  - Ein Migrationsplan mit mindestens zwei Inkrementen (z. B. Read-only Shadow-Betrieb, danach produktiver Dual-UM980-Betrieb) ist definiert.
  - Test- und Validierungsstrategie für Labor und Feldtest ist enthalten (inkl. RTK-Fix-Verhalten, Latenz, Ausfall eines Moduls).
  - Offene Entscheidungen (z. B. Priorisierung primäres/sekundäres Modul, Umschaltlogik) sind als explizite Decision-Points dokumentiert.
- **Owner**: ki-planer
- **Links**:
  - `backlog/epics/EPIC-004-feature-expansion.md`
  - `backlog/tasks/TASK-013-rtcm-zu-um980-validierung.md`
  - `backlog/tasks/TASK-014-hal-gnss-rtcm-uart-forwarding.md`
  - `backlog/tasks/TASK-015-udp-rtcm-receiver-und-buffering.md`
  - `backlog/tasks/TASK-017-rtcm-validierung-agiou-m980.md`
  - `docs/Handover2.md#8-offene-aufgaben--todos`
- **delivery_mode**: mixed
- **task_category**: feature_expansion


## Planer-Update (2026-04-17)

- Konsolidierung der gemergten Umsetzung TASK-019A..019E abgeschlossen (Status `done`).
- Restumfang für TASK-019 bleibt **offen** und fokussiert auf:
  - produktive Dual-UM980-Failover-Logik im Runtime-Pfad,
  - Labor-/Feldvalidierung mit reproduzierbaren Messkriterien.
- Folge-Tasks: `TASK-019F` (Failover-Logik) und `TASK-019G` (Labor-/Feldvalidierung).
