# TASK-037 Basis-Task: Einheitlichen Konfig-Framework-Rahmen vor Feature-Umsetzung festlegen

- **ID**: TASK-037
- **Titel**: Basis-Task für einheitlichen Konfig-Framework-Rahmen vor NTRIP/UART/GNSS/Ethernet
- **Status**: open
- **Priorität**: high
- **Komponenten**: `docs/`, `backlog/tasks/`, `backlog/index.yaml`, ggf. `docs/adr/`
- **Dependencies**: []
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer
- **Epic**: EPIC-003

- **classification**: foundational
- **exclusive_before**:
  - TASK-NTRIP-CONFIG
  - TASK-UART-CONFIG
  - TASK-GNSS-CONFIG
  - TASK-ETHERNET-CONFIG
  - TASK-CONFIG-INTEGRATION
- **parallelizable_after**:
  - TASK-NTRIP-CONFIG
  - TASK-UART-CONFIG
  - TASK-GNSS-CONFIG
  - TASK-ETHERNET-CONFIG
  - TASK-CONFIG-INTEGRATION

- **Origin**:
  Nutzeranforderung aus Chat (2026-04-21):
  Vor der Umsetzung der fünf Feature-Tasks muss ein gemeinsamer Basis-Task erstellt werden,
  der den einheitlichen Konfig-Framework-Rahmen verbindlich festlegt.

## Pflichtinstruktion vor Umsetzung

1. **`README.md` lesen und einhalten.**
2. **`agents.md` lesen und einhalten.**

Ohne diese Pflichtlektüre darf keine Implementierung dieses Tasks oder nachgelagerter
Feature-Tasks beginnen.

## Kontext / Problem

Für Konfigurationsänderungen in NTRIP/UART/GNSS/Ethernet fehlt derzeit ein einheitlicher,
vorab fixierter Rahmen für:
- Menüführung und Zustandsübergänge,
- Validierungsregeln und Fehlermeldungen,
- Persistenz (inkl. Defaults und Migration),
- eindeutige Abhängigkeitsreihenfolge zwischen Basis und Feature-Tasks.

Ohne diesen Basisrahmen drohen inkonsistente UI-/State-Verhalten und divergierende
Speicher-/Migrationspfade zwischen den Feature-Implementierungen.

## Scope (in)

- Einheitliches **Menü- und Zustandsmodell** spezifizieren:
  - Navigation zwischen Konfigseiten,
  - Save/Discard-Verhalten,
  - Exit/Boot-Continue-Entscheidungspfad.
- Zentrale **Validierungsregeln** und ein verbindlicher **Fehlermeldungsstil** definieren.
- **Persistenzstrategie** festlegen:
  - atomarer/sicherer Schreibpfad,
  - Umgang mit Defaults,
  - Migration bestehender Konfiguration.
- **Dependency-Reihenfolge** verbindlich dokumentieren:
  1. Basis-Task (dieser Task) zuerst,
  2. danach Feature-Tasks in der Reihenfolge NTRIP → UART → GNSS → Ethernet,
  3. abschließend Integrations-/Abnahmetask.
- Entscheidung zum **ADR-Bedarf** explizit dokumentieren.

## Nicht-Scope (out)

- Keine direkte Implementierung der NTRIP/UART/GNSS/Ethernet-Features.
- Kein Umbau produktiver Laufzeitlogik außerhalb der Rahmenfestlegung.
- Keine hardware-spezifischen Tuning-Entscheidungen pro Board.

## Verbindliches Rahmenmodell

### A) Gemeinsames Menü- und Zustandsmodell

- Einheitlicher Zustandsautomat für alle Konfigbereiche mit minimal folgenden Zuständen:
  - `VIEW` (lesen/navigieren),
  - `EDIT_DIRTY` (Änderungen vorhanden, noch nicht persistiert),
  - `VALIDATING` (prüfen),
  - `SAVE_PENDING` (atomare Persistenz läuft),
  - `SAVE_OK` / `SAVE_FAILED`.
- Einheitliche Navigation:
  - `Next/Prev` innerhalb eines Bereichs,
  - `Back` zur vorherigen Ebene,
  - `Home` zur Startansicht.
- Einheitliches Verhalten für Aktionen:
  - **Save**: nur bei validem Zustand, sonst blockiert mit Feld-/Regelhinweis.
  - **Discard**: verwirft alle seit letztem Save geänderten Felder im aktuellen Scope.
  - **Exit**: bei `EDIT_DIRTY` immer expliziter Dialog `Save | Discard | Cancel`.
  - **Boot-Continue**: nur bei abgeschlossenem oder bewusst verworfenem Zustand erlaubt.

### B) Zentrale Validierungsregeln + Fehlermeldungsstil

- Validierung als zentrale Regelbibliothek, nicht verteilt in einzelnen Feature-Dateien.
- Regeln mindestens in Klassen einteilen:
  1. `required` (Pflichtfelder),
  2. `format` (z. B. Hostname/IP, numerische Formate),
  3. `range` (Grenzwerte),
  4. `dependency` (feature-übergreifende Abhängigkeiten).
- Fehlermeldungsstil verbindlich:
  - Struktur: `CODE | Kurztext | Feld | Handlungshinweis`.
  - Beispiele:
    - `CFG-RANGE-001 | Wert außerhalb Bereich | gnss.baud | Erlaubt: 4800..921600`
    - `CFG-DEP-002 | Abhängigkeit nicht erfüllt | ntrip.enabled | Ethernet zuerst aktivieren`
- Fehlertexte müssen reproduzierbar, maschinenlesbar (Code) und nutzerverständlich sein.

### C) Persistenzstrategie (atomar/sicher, Defaults, Migration)

- Persistenz muss atomar erfolgen (Write-temp + Verify + Commit/Swap).
- Bei Fehlern keine teilweise geschriebene aktive Konfiguration übernehmen.
- Defaults:
  - Kaltstart ohne gültige User-Config lädt bekannte `cfg::*`-Defaults.
  - Defaults sind versioniert und als Basis für Migration nutzbar.
- Migration:
  - Jede persistierte Konfiguration trägt `schema_version`.
  - Migrationspfad `vN -> vN+1` ist deterministisch und dokumentiert.
  - Nicht migrierbare/ungültige Teilbereiche fallen kontrolliert auf Defaults zurück
    (mit Warnlog, nicht mit stillem Verhalten).

### D) Dependency-Reihenfolge (verbindlich)

1. **TASK-037 (dieser Basis-Task)**
2. **Feature-Task 1:** NTRIP-Konfiguration
3. **Feature-Task 2:** UART-Konfiguration
4. **Feature-Task 3:** GNSS-Konfiguration
5. **Feature-Task 4:** Ethernet-Konfiguration
6. **Feature-Task 5:** Integrations-/Abnahmetask über alle vier Bereiche

Kein Feature-Task darf auf `in_progress`/`done` gehen, bevor TASK-037 mindestens `done` ist.

## ADR-Bedarf: Prüfung und explizite Entscheidung

- **Prüfung:** Der Basisrahmen definiert repo-weite, dauerhafte Invarianten für
  Konfig-Zustandsmodell, Validierung und Persistenz.
- **Entscheidung:** **Ja, ADR erforderlich.**
- **Vorgabe:** Vor oder spätestens zusammen mit der Umsetzung dieses Tasks muss ein ADR
  erstellt oder ein bestehender ADR erweitert werden, der folgende Punkte normativ festhält:
  1. globales Konfig-Zustandsmodell,
  2. zentrale Validierungsarchitektur,
  3. atomare Persistenz + Migrationspolitik,
  4. Konflikt-/Fallback-Regeln bei fehlerhafter Konfiguration.

## files_read

- `README.md`
- `agents.md`
- `docs/adr/ADR-001-config-layering-fw-soft-runtime.md`
- `docs/adr/README.md`
- `backlog/index.yaml`
- relevante Folge-Tasks zu NTRIP/UART/GNSS/Ethernet (sobald angelegt)

## files_write

- `backlog/tasks/TASK-037-basis-konfig-framework-rahmen-vor-feature-tasks.md`
- `backlog/index.yaml`
- optional neuer/erweiterter ADR unter `docs/adr/` (Folgeschritt)

## Invarianten

- Ein gemeinsamer Rahmen ist vor Feature-Implementierung verbindlich festgelegt.
- Save/Discard/Exit/Boot-Continue verhalten sich in allen Konfigbereichen gleich.
- Validierung und Fehlermeldungen sind zentral und konsistent.
- Persistenz ist atomar und migrationsfähig.

## Known traps

- Verteilte Ad-hoc-Validierung in einzelnen Feature-Dateien unterläuft zentrale Regeln.
- Uneinheitliche Exit-Dialoge führen zu impliziten Datenverlustpfaden.
- Migration ohne klare `schema_version` erzeugt schwer reproduzierbare Feldzustände.

## AC

1. Pflichtinstruktion (`README.md` + `agents.md` lesen/einhalten) ist im Task explizit enthalten.
2. Gemeinsames Menü- und Zustandsmodell (Navigation, Save/Discard, Exit/Boot-Continue)
   ist vollständig spezifiziert.
3. Zentrale Validierungsregeln und Fehlermeldungsstil sind verbindlich dokumentiert.
4. Persistenzstrategie (atomar/sicher, Defaults, Migration) ist dokumentiert.
5. Dependency-Reihenfolge ist explizit: Basis-Task zuerst, dann NTRIP/UART/GNSS/Ethernet,
   danach Integrations-/Abnahmetask.
6. ADR-Bedarf ist geprüft und Entscheidung explizit dokumentiert.

## verification

- `rg "Pflichtinstruktion|Menü- und Zustandsmodell|Validierungsregeln|Persistenzstrategie|Dependency-Reihenfolge|ADR-Bedarf" backlog/tasks/TASK-037-basis-konfig-framework-rahmen-vor-feature-tasks.md`
- `yq '.tasks[] | select(.id == "TASK-037")' backlog/index.yaml` (falls verfügbar)
- Alternativ: `rg "TASK-037" backlog/index.yaml`

## Links

- `README.md`
- `agents.md`
- `docs/adr/ADR-001-config-layering-fw-soft-runtime.md`
- `docs/adr/README.md`
