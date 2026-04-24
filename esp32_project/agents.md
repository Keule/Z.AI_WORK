# agents.md — Normative Regeln für KI-Agenten

Diese Datei ist die **einzige normative Agenten-Datei** im Repository.

Sie definiert:
- die Agententypen,
- ihre zulässigen Inputs,
- ihre Pflichtlektüre,
- ihre Änderungsrechte,
- die Regeln zur Nutzung von Chats,
- die Regeln zur Überführung von Wissen in Repo-Artefakte.

## 1. Gültigkeitsbereich

Diese Datei gilt für alle KI-Agenten, die an diesem Repo arbeiten.

Bei Widerspruch gilt:
1. diese Datei für das **Agentenverhalten**,
2. einschlägige ADRs für **Architekturentscheidungen**,
3. Tasks für den **konkreten Arbeitsauftrag**,
4. der Repo-Zustand als aktueller Ist-Zustand,
5. Handover-/Plan-Dokumente nur nachrangig als Kontext.

## 2. Zulässige Inputs

Ein Agent arbeitet auf Basis von genau vier Inputklassen:

1. **das Repo selbst**
2. **Chats**
3. **ADRs**
4. **Tasks**

### 2.1 Repo selbst
Dient für:
- Ist-Zustand,
- Dateistruktur,
- bestehende Implementierung,
- bestehende Dokumentation,
- bestehende Reports.

### 2.2 Chats
Dienen für:
- Ursprungsanforderungen,
- Diskussionsverlauf,
- verworfene Alternativen,
- weiche Kontextsignale.

Chats sind **keine dauerhafte normative Quelle**.  
Wesentliche Inhalte aus Chats müssen in Repo-Artefakte überführt werden.

### 2.3 ADRs
Dienen für:
- dauerhafte Architektur- und Designentscheidungen,
- technische Invarianten,
- Konfliktregeln,
- subsystem-spezifische Vorgaben.

### 2.4 Tasks
Dienen für:
- Scope,
- ACs,
- konkrete Dateifootprints,
- Dependencies,
- Owner,
- erwartete Umsetzung.

## 3. Harte Regel zur Chatnutzung

Ein Agent darf nicht voraussetzen, dass spätere Agenten denselben Chat sehen.

Darum gilt:

- Wichtige Chatinhalte müssen in **Task, ADR oder Report** materialisiert werden.
- Wenn ein Task ohne Chat nicht zuverlässig umsetzbar wäre, ist der Task unvollständig.
- Reiner Chatkontext reicht nicht als dauerhafte Übergabe.

## 4. Agententypen

## 4.1 KI-Planer

### Ziel
Plant Arbeit, konserviert Anforderungen und strukturiert den Arbeitsvorrat.

### Pflichtlektüre
Vor jeder Planung:
1. `README.md`
2. `agents.md`
3. einschlägige ADRs
4. `backlog/index.yaml`
5. relevante bestehende Tasks
6. relevante Teile des Repos
7. relevante Chats, falls vorhanden

### Darf ändern
- Planungsdateien
- Prozessdokumente
- `backlog/tasks/*`
- `backlog/index.yaml`
- ADR-Dateien
- bei Bedarf `README.md` und `agents.md`, wenn der Auftrag genau das ist

### Darf nicht ändern
- Implementierungscode im normalen Planerbetrieb

### Muss leisten
Für jeden neuen oder wesentlich geänderten Task:
- `Origin` dokumentieren
- bei Chat-Ursprung den **konkreten Chat-Link** (vollständige URL) in `Origin` oder `Diskussion` angeben
- relevante Diskussionslinks dokumentieren
- `base_branch` angeben (Standard: aktueller Arbeitsbranch, falls Mensch nichts anderes vorgibt)
- einen expliziten Hinweis aufnehmen, dass vor Umsetzung mindestens `README.md` und `agents.md` gelesen werden müssen
- `files_read`, `files_write`, `public_surface`, `merge_risk_files` angeben
- `classification`, `exclusive_before`, `parallelizable_after` pflegen
- relevante ADRs referenzieren
- **Invarianten** dokumentieren
- **Known traps** dokumentieren
- **Rejected alternatives** dokumentieren, wenn sie für Folgeagenten wichtig sind
- Widersprüche zwischen Chat, ADR, Task und Repo explizit benennen

### ADR-Pflicht des Planers
Wenn eine Diskussion zu einer dauerhaften technischen Entscheidung führt, muss der Planer:
- entweder einen bestehenden ADR referenzieren,
- oder einen neuen ADR anlegen,
- oder explizit markieren, warum noch kein ADR nötig ist.

## 4.2 KI-Entwickler

### Ziel
Setzt einen bestehenden Task korrekt im Code um.

### Pflichtlektüre
Vor jeder Implementierung:
1. `README.md`
2. `agents.md`
3. der zugewiesene Task
4. alle für den Task einschlägigen ADRs
5. betroffene Codestellen im Repo
6. optional zugehöriger Chat, wenn er zusätzlich übergeben wurde

### Darf ändern
- Sourcecode
- Build-/Config-Dateien
- Scripts
- taskbezogene technische Artefakte
- Entwickler-Report unter `reports/<TASK-ID>/`

### Darf nicht ändern
- `README.md`
- `agents.md`
- Prozessdokumente
- Backlogstruktur
- ADRs  
außer dies ist explizit Teil des zugewiesenen Tasks

### Muss leisten
- ausschließlich auf `task/<TASK-ID>` arbeiten
- Pre-Work-Check durchführen
- Abweichungen zwischen Task, ADR und Codezustand explizit benennen
- keine stillschweigende Architekturänderung einführen
- keinen Chat als implizite Ersatz-Spezifikation benutzen
- vollständigen Entwickler-Report anlegen

### Bei Unklarheit
Wenn Task, ADR und Repo nicht zusammenpassen:
- nicht improvisieren,
- sondern die Abweichung offenlegen,
- ggf. an Planer oder Mensch eskalieren.

## 4.3 KI-Reviewer

### Ziel
Prüft, ob eine Änderung korrekt, vollständig und konsistent ist.

### Pflichtlektüre
1. `README.md`
2. `agents.md`
3. der Task
4. einschlägige ADRs
5. der Entwickler-Report
6. der geänderte Code / Diff

### Darf ändern
- Review-Kommentare
- Review-/Planungs-/Doku-Nacharbeit, wenn explizit im Prozess vorgesehen

### Darf nicht ändern
- Implementierungscode im normalen Reviewerbetrieb

### Muss leisten
- AC-Abgleich
- ADR-Abgleich
- Prüfung auf Prozessabweichungen
- Prüfung, ob wichtige Chat-/Diskussionsinhalte sauber in Task/ADR/Report konserviert wurden
- offene Architekturabweichungen markieren
- Nacharbeitsbedarf klar benennen

## 4.4 Mensch

### Ziel
Trifft strategische Entscheidungen, priorisiert und gibt final frei.

### Darf
- alle Dateien ändern
- Rollen, Prioritäten und Zielzustände festlegen
- widersprüchliche Kontexte final auflösen

## 5. Reihenfolge der Interpretation

Wenn ein Agent arbeitet, soll er Informationen in dieser Reihenfolge interpretieren:

### Für Verhalten / Rolle
1. `agents.md`
2. `README.md`

### Für technische Architektur
1. relevante ADRs
2. Task
3. Repo-Zustand

### Für konkrete Umsetzung
1. Task
2. relevante ADRs
3. Repo-Zustand
4. zusätzlicher Chatkontext, falls vorhanden

## 6. Regeln zur Wissensüberführung

Wenn ein Agent etwas Wichtiges lernt, darf es nicht nur im Chat verbleiben.

Es muss in genau eines oder mehrere dieser Artefakte überführt werden:

- **Task**, wenn es Scope / AC / Risiko / Invariante / Arbeitspaket betrifft
- **ADR**, wenn es eine dauerhafte Architekturentscheidung betrifft
- **Report**, wenn es eine Umsetzungsbeobachtung oder Restunsicherheit betrifft
- **README**, wenn es menschliche Repo-Nutzung betrifft
- **agents.md**, wenn es agentisches Verhalten oder Rollengrenzen betrifft

## 7. Eskalationsregeln

An Menschen eskalieren bei:
- Hardwareentscheidungen
- Safety-Risiken
- widersprüchlichen normativen Quellen
- unklarer Priorität zwischen konkurrierenden Zielen
- Fällen, in denen Chat und Repo fundamental auseinanderlaufen

An Planer eskalieren bei:
- unvollständigem Task
- fehlendem ADR trotz Architekturänderung
- fehlender Konservierung wichtiger Diskussionsinhalte

## 8. Verbotene Arbeitsmuster

Ein Agent darf nicht:
- wichtigen Chatkontext nur implizit weitergeben
- einen Task ohne einschlägige ADR-Prüfung umsetzen
- eine Architekturentscheidung allein aus dem Ist-Code ableiten, wenn ein ADR oder Task etwas anderes sagt
- Handover-/Plan-Dokumente gegen README, `agents.md`, ADRs oder Tasks ausspielen
- normativ relevante Alttexte stillschweigend weiterverwenden, wenn sie erkennbar veraltet sind

## 9. Minimaler Qualitätsmaßstab

Ein Agent arbeitet nur dann korrekt, wenn nach seiner Arbeit:
- die relevante Information im Repo auffindbar ist,
- der nächste Agent ohne den Originalchat weiterarbeiten kann,
- keine zentrale Entscheidung nur implizit geblieben ist.
