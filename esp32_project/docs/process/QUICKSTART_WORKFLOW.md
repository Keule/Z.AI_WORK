# Quickstart Workflow (Mensch + Agent)

Kurzleitfaden für sauberes Arbeiten über Chats/Agenten/Umgebungen hinweg.

## 1) Session-Start (immer gleich)
1. Einstieg lesen:
   - `README.md`
   - `docs/process/PLAN_AGENT.md`
   - `backlog/README.md`
2. Task auswählen oder anlegen:
   - **Nur der KI-Planer** darf neue Tasks anlegen (Datei in `backlog/tasks/` + Eintrag in `backlog/index.yaml`).
   - KI-Entwickler dürfen ausschließlich bestehende `TASK-XXX` auswählen und bearbeiten.
3. Scope festziehen:
   - Was ist in scope / out of scope?
   - Welche Dateien werden voraussichtlich geändert?

## 1a) Pre-Work-Check (verpflichtend, vor der ersten Änderung)
1. Task-Kontext prüfen (Task-ID einsetzen):
   - `python3 tools/check_task_context.py --task-id <Task-ID>`
2. Erwartetes Ergebnis:
   - aktueller Branch ist exakt `task/<Task-ID>`.
   - Verzeichnis `reports/<Task-ID>/` existiert.
3. Bei Abweichung **sofort stoppen und eskalieren**:
   - falscher Branch (z. B. `gnss_integration`) ist für Task-Arbeit nicht zulässig.
   - fehlender Report-Ordner muss vor Start angelegt oder durch Planer/Verantwortliche geklärt werden.


## 1b) Pflicht-Onboarding in Codex-Task-Buttons (verbindlich)
Wenn Aufgaben über klickbare Codex-Buttons/Prompt-Vorlagen gestartet werden, MUSS der Prompt standardmäßig den Pflicht-Onboarding-Block enthalten:
1. `README.md` (Agenten-Teil)
2. `docs/process/PLAN_AGENT.md`
3. `docs/process/QUICKSTART_WORKFLOW.md`

Regel:
- Fehlt der Block im Button-Prompt, gilt der Start als Prozessabweichung und ist wie in **2e) Prozessabweichung** nachzuarbeiten.
- Der Onboarding-Block darf durch Template-Refactoring oder UI-Umformatierung nicht entfernt werden (Regression Guard).

## 2) Planung vor Umsetzung
1. Task klassifizieren:
   - `independent` oder `dependent`.
2. Abhängigkeiten prüfen:
   - `exclusive_before` und `parallelizable_after` festlegen (wenn nötig).
3. Konfliktflächen prüfen:
   - `merge_risk_files` identifizieren.
4. Claim/Lock setzen:
   - kritische Dateien exklusiv claimen, bevor implementiert wird.

## 2a) Verbindliche Backlog-Regeln (muss vor jeder Implementierung erfolgen)
1. **Footprint-Analyse Pflicht**
   - `files_read`, `files_write`, `public_surface` dokumentieren.
2. **Task-Klassifikation**
   - Jede Aufgabe als `independent` oder `dependent` markieren.
   - Bei `dependent`: `exclusive_before` oder `parallelizable_after` festlegen.
3. **Merge-Risiko aktiv managen**
   - `merge_risk_files` identifizieren.
   - Vor Änderung claimen/locken.
   - Nur geclaimte Pfade ändern.
4. **Commits**
   - klein, nachvollziehbar, taskbezogen.

## 2b) Protokollgrenze explizit dokumentieren
Wenn eine Aufgabe den Protokollpfad berührt, MUSS im Task stehen:
- ob Core-AOG-CRC-Strictness betroffen ist,
- ob Discovery/Management-Ausnahmen betroffen sind,
- was unverändert bleibt und warum.

Regel: Core-AOG-Pfad bleibt CRC-strikt, Discovery/Management-Pfad bleibt AgIO-kompatibel tolerant.

## 2c) Backlog-Einträge korrekt anlegen
Jeder Task bekommt eine eigene Datei:
- `backlog/tasks/TASK-xxxx-<slug>.md`

Pflichtfelder:
- ID
- Titel
- Status (`open | in_progress | blocked | done`)
- Priorität
- Komponenten
- Dependencies
- Acceptance Criteria
- Owner
- delivery_mode

Anschließend im `backlog/index.yaml` registrieren.

## 2d) Eskalation und Normativität
1. **An Menschen eskalieren**, wenn:
   - Hardware-Entscheidungen nötig sind,
   - Safety-Risiken auftreten,
   - unerwartete Architektur-/Protokollabweichungen auftreten.
2. **Handover ist Kontext, nicht Spezifikation**
   - Normative Regeln stehen in `README.md`, Prozess-, Architektur- und Protokolldokumenten.
   - Handover-Dateien dienen zur Übergabe, nicht als verbindliche Quelle.


## 2e) Prozessabweichung
**Trigger:**
- falscher Branch,
- fehlender Entwickler-Report,
- Rollenverstoß (z. B. KI-Entwickler ändert Prozess-/Doku-Dateien).

**Pflichtreaktion:**
- Der KI-Planer legt umgehend einen Nacharbeits-Task an.
- Der Task enthält mindestens: klare AC, Owner und eine konkrete Deadline.
- Der Task wird in `backlog/index.yaml` sichtbar registriert und bis zur Erledigung nachverfolgt.

## 3) Umsetzung
1. Kleine, nachvollziehbare Änderungen.
2. Architekturgrenzen respektieren:
   - Core-AOG-PGNs: CRC strikt.
   - Discovery/Management-Pfad: definierte CRC-Ausnahme.
3. Konventionen einhalten:
   - HAL-/Logic-Trennung,
   - Logging-Regeln,
   - State-Zugriffe mit Mutex.

## 4) Dokumentation während der Arbeit
1. Session-Templates nutzen:
   - `templates/session-start.md`
   - `templates/session-progress.md`
2. Entwickler-Report pro Task verpflichtend:
   - Report unter `reports/<Task-ID>/<dev-name>.md` anlegen.
   - Verbindliches Template: `templates/dev-report.md`.
   - Auswertung/Integration erfolgt durch Reviewer/Mensch.
3. Bei Architektur-/Prozessentscheidungen:
   - ADR erfassen (`templates/adr.md`).
4. Rollenregel für Änderungen:
   - KI-Entwickler ändern keine Prozess- oder Doku-Dateien.
   - Prozess-/Doku-Änderungen obliegen KI-Planer, KI-Reviewer und Mensch.

## 4a) Definition of Done (KI-Entwickler) — verbindlich
Ein Task gilt für KI-Entwickler **nur dann** als `done`, wenn ein vollständiger Entwickler-Report vorhanden ist:

- Pflicht-Artefakt: `reports/<Task-ID>/<dev-name>.md`
- Verbindliche Vorlage: `templates/dev-report.md`
- Ohne Report-Datei darf der Task-Status **nicht** `done` sein.
- Fehlender Report ⇒ Task-Status mindestens `blocked` (nicht `done`).

## 5) Session-Ende / Handover
1. `templates/session-handover.md` ausfüllen:
   - erledigt / offen / blocker / nächster konkreter Schritt.
2. Task-Status aktualisieren.
3. Referenzen dokumentieren:
   - betroffene Dateien, Commits/PR, offene Risiken.

## 5a) Optionaler PR-/CI-Check für Task-Branches
- Ziel: PRs mit Task-Arbeit auf `gnss_integration` verhindern.
- Manuell vor PR:
  - `python3 tools/check_task_context.py --task-id <Task-ID>`
- Optional in CI (Beispielidee):
  - bei Task-PRs den Check als Pflichtschritt ausführen,
  - Build fehlschlagen lassen, wenn Branch nicht `task/<Task-ID>` ist oder `reports/<Task-ID>/` fehlt.

## 6) Umgebungswechsel (Codex ↔ ChatGPT ↔ z.ai)
Beim Wechsel immer aktiv mitgeben:
- Task-ID
- aktueller Status
- geänderte Dateien
- offene Blocker
- nächster konkreter Schritt

Nie auf Chat-Kontext verlassen — der aktuelle Stand muss im Repo stehen (Task/Handover/ADR).git 
