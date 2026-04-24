# PLAN_AGENT — Parallelisierungs- und Merge-Regeln

Dieses Dokument definiert, wie ein Plan-Agent Aufgaben in **unabhängige** und **abhängige** Arbeitspakete zerlegt, Risiken früh erkennt und Konflikte bei paralleler Bearbeitung vermeidet.

## Ziel

- Parallele Arbeit ermöglichen, ohne Integrationschaos zu erzeugen.
- Dateikonflikte und semantische Kollisionen früh sichtbar machen.
- Klare Regeln für Reihenfolge (`exclusive_before`, `parallelizable_after`) und Besitz von Dateien (Claiming/Locking) setzen.

---

## Rollen und Verantwortlichkeiten

### Mensch

- Trifft strategische Entscheidungen und priorisiert den Entwicklungszweig.
- Bestätigt finale Merges in den Entwicklungszweig.
- Pflegt übergreifende Dokumentation und Backlog bei Bedarf.
- Darf Prozess- und Doku-Dateien ändern.

### KI-Planer

- Legt als einzige Rolle neue Tasks im Backlog an (`backlog/tasks/` + `backlog/index.yaml`).
- Klassifiziert Tasks (`independent`/`dependent`) und setzt `exclusive_before`/`parallelizable_after`.
- Identifiziert Dateifootprints und `merge_risk_files`.
- Erstellt pro Task einen Branch `task/<Task-ID>` vom aktuellen Entwicklungszweig.
- Darf Planungs-, Prozess- und Backlog-Dateien ändern.
- Darf keine Code-Dateien ändern.

#### Verhaltensregeln bei Task-Erstellung

1. **Repo-Kontext studieren** — Vor jeder Task-Erstellung muss der KI-Planer die aktuelle Repo-Struktur, Backlog-Konventionen (`backlog/README.md`, `backlog/index.yaml`), bestehende Tasks und `README.md` gelesen haben. Die Task-Nummerierung, Namenskonventionen und Epic-Zuordnung müssen dem aktuellen Stand entsprechen.
2. **Origin dokumentieren** — Jeder Task muss ein `Origin`-Feld enthalten, das die Nutzer-Fragen und -Anforderungen zusammenfasst, die zu dem Task geführt haben. Dies dient der Nachvollziehbarkeit und dem Kontext-Transfer.
3. **Diskussions-Links** — Jeder Task muss Links zur zugrundeliegenden Diskussion enthalten:
   - **Direkt-Link**: Link zur spezifischen Chat-Session (z. B. `https://chat.z.ai/c/<chat-id>`).
   - **Shared-Link**: Shared-Version der Diskussion (z. B. `https://chat.z.ai/s/<share-id>`).
   Beide Links stellen sicher, dass der volle Diskussionkontext für KI-Entwickler und Reviewer zugänglich ist.

### KI-Entwickler

- Arbeitet ausschließlich auf einem zugewiesenen `task/<Task-ID>`-Branch.
- Bei Start über Codex-Task-Buttons muss der Prompt verpflichtend den Onboarding-Block enthalten (`README.md`, `docs/process/PLAN_AGENT.md`, `docs/process/QUICKSTART_WORKFLOW.md`).
- Darf nur Code-Artefakte ändern (Source, Skripte, Konfiguration).
- Darf keine Prozess- oder Doku-Dateien ändern.
- Muss pro Task einen Report unter `reports/<Task-ID>/<dev-name>.md` anlegen und dafür das verbindliche Template `templates/dev-report.md` verwenden.

### KI-Reviewer

- Prüft PRs der Entwickler gegen `task/<Task-ID>`.
- Extrahiert Informationen aus den Entwickler-Reports und integriert sie in Doku/Backlog.
- Darf keine Codeänderungen vornehmen.

## Task- und Branch-Workflow

### Verbindlicher Start-Handshake (vor jeder Implementierung)

1. **KI-Planer erstellt den Task-Branch:** `task/<Task-ID>` vom aktuellen Entwicklungszweig.
2. **KI-Entwickler bestätigt den exakten Branchnamen**, bevor die erste Codeänderung erfolgt (z. B. über `git branch --show-current` oder den Repo-Check unter `tools/check_task_context.py`).
3. **Ohne bestätigten Branchname keine Umsetzung starten.** Bei Abweichung sofort stoppen und an Mensch/KI-Planer eskalieren.

1. Der KI-Planer erstellt die Task-Datei in `backlog/tasks/` und ergänzt den Eintrag in `backlog/index.yaml`; dabei werden `independent`/`dependent` sowie `exclusive_before`/`parallelizable_after` gesetzt.
2. Der KI-Planer erstellt den Branch `task/<Task-ID>` vom aktuellen Entwicklungszweig.
3. KI-Entwickler checken den Task-Branch aus, ändern ausschließlich Code und legen ihren Report unter `reports/<Task-ID>/<dev-name>.md` gemäß `templates/dev-report.md` an.
4. KI-Entwickler eröffnen Pull Requests gegen den jeweiligen Task-Branch (nicht gegen `main` oder `gnss_integration`).
5. Vor PR-Freigabe prüfen KI-Reviewer und/oder KI-Planer explizit:
   - Existiert eine Report-Datei für die Task-ID unter `reports/<Task-ID>/<dev-name>.md`?
   - Ist das Template vollständig ausgefüllt (mindestens: Zusammenfassung, geänderte Dateien, Tests/Build, offene Fragen)?
   - Falls nein: PR nicht freigeben, Task-Status mindestens `blocked` belassen.
6. KI-Reviewer und Mensch sichten die PRs, werten die Reports aus, integrieren die Erkenntnisse in Dokumentation/Backlog und schließen den Task.
7. Nach Abschluss wird der Task-Branch in den Entwicklungszweig (`gnss_integration`) gemergt.

---

## 1) Datei-Footprint-Analyse

Vor jeder Planung muss ein **Datei-Footprint** je Task erstellt werden.

### Pflichtinhalt pro Task

- `task_id`: Eindeutige Kennung.
- `change_intent`: Kurzbeschreibung der beabsichtigten Änderung.
- `files_read`: Dateien, die nur gelesen werden.
- `files_write`: Dateien, die geändert/erstellt/gelöscht werden.
- `public_surface`: Betroffene öffentliche Schnittstellen (z. B. Header, PGN-Schema, API-Verträge, Build-Konfiguration).
- `risk_notes`: Bekannte Integrations- oder Regression-Risiken.

### Klassifikation

- **Narrow footprint**: wenige, lokal isolierte Dateien.
- **Wide footprint**: mehrere Module, zentrale Konfigs oder öffentliche Schnittstellen.
- **Hotspot footprint**: Dateien, die häufig von mehreren Tasks berührt werden (z. B. `README.md`, zentrale Header, Index-Dateien).

---

## 2) independent vs. dependent

Jeder Task muss explizit markiert werden:

- `independent`: Kann ohne Vorbedingung gestartet werden und bricht keine erwartete Schnittstelle anderer laufender Tasks.
- `dependent`: Benötigt Vorarbeiten anderer Tasks (z. B. API-Freeze, Datenmodell, Migrationsschritt).

### Heuristik

Ein Task ist **dependent**, wenn mindestens eine Bedingung zutrifft:

1. Er konsumiert eine Schnittstelle, die im selben Zyklus noch geändert wird.
2. Er schreibt in Dateien aus `merge_risk_files` (siehe unten), die bereits von einem anderen Task geclaimt sind.
3. Sein Test-/Build-Erfolg hängt von Artefakten eines anderen offenen Tasks ab.

---

## 3) `exclusive_before`

`exclusive_before` listet Tasks auf, die **zwingend abgeschlossen** sein müssen, bevor der aktuelle Task startet.

### Regel

Wenn `exclusive_before` nicht leer ist:

- Task darf nicht parallel gestartet werden.
- Start erst nach Merge/Übernahme der referenzierten Tasks.
- Re-Check der Datei-Footprint-Analyse nach Abschluss der Vorbedingung.

Beispiel:

```yaml
exclusive_before:
  - TASK-API-001
  - TASK-SCHEMA-003
```

---

## 4) `parallelizable_after`

`parallelizable_after` listet minimale Vorbedingungen, nach deren Abschluss ein Task **parallel** zu anderen laufen darf.

### Regel

- Sobald alle IDs in `parallelizable_after` erledigt sind, darf der Task in den Parallel-Pool.
- Trotzdem bleibt Claiming/Locking auf Dateiebene verpflichtend.

Beispiel:

```yaml
parallelizable_after:
  - TASK-API-001
```

---

## 5) `merge_risk_files`

`merge_risk_files` ist die projektspezifische Liste von Dateien mit hohem Konfliktpotenzial.

### Typische Kandidaten

- `README.md`
- zentrale Build-/CI-Dateien
- globale Konfigurationsdateien
- zentrale Header/Interfaces
- Index-/Registry-Dateien

### Regel

- Änderungen an `merge_risk_files` müssen früh angekündigt und exklusiv geclaimt werden.
- Wenn möglich, Änderungsfenster bündeln (z. B. „Docs-Slot", „API-Slot“).
- Bei unvermeidbarer Parallelität: kleinste mögliche Diffs, häufiges Rebase.

---

## 6) Claiming/Locking

Claiming/Locking verhindert, dass zwei Tasks dieselbe kritische Datei gleichzeitig „besitzen".

### Claim-Lebenszyklus

1. **Claim anlegen**
   - Task meldet `files_write` + geplante Dauer.
2. **Lock prüfen**
   - Kollision mit bestehendem Claim? Dann:
     - entweder warten,
     - oder Task neu schneiden (Footprint verkleinern),
     - oder explizite Reihenfolge setzen (`exclusive_before`).
3. **Bearbeiten**
   - Nur geclaimte Schreibpfade ändern.
4. **Freigeben**
   - Nach Merge oder Abbruch Claim sofort entfernen.

### Mindestregeln

- Kein Commit gegen aktive fremde Locks in derselben Datei.
- Für `merge_risk_files` ist ein **exklusiver Lock** obligatorisch.
- Locks sind kurzlebig zu halten; lange Locks in kleinere Teilaufgaben aufteilen.

---

## 7) Empfohlenes Plan-Schema (YAML)

```yaml
task_id: TASK-XYZ-123
change_intent: "Kurzbeschreibung"
classification: independent # oder dependent
exclusive_before: []
parallelizable_after: [TASK-API-001]
files_read:
  - src/module_a.cpp
files_write:
  - docs/process/PLAN_AGENT.md
public_surface:
  - docs/process/PLAN_AGENT.md
merge_risk_files:
  - README.md
risk_notes:
  - "README-Konflikte bei parallelen Doku-Tasks möglich"
```

---

## 8) Entscheidungslogik (Kurzform)

1. Datei-Footprint analysieren.
2. `independent`/`dependent` setzen.
3. Falls nötig `exclusive_before` definieren.
4. Sonst `parallelizable_after` setzen.
5. Betroffene `merge_risk_files` claimen/locken.
6. Erst danach Implementierung starten.

Diese Reihenfolge ist verbindlich für parallele Planung und Ausführung.
