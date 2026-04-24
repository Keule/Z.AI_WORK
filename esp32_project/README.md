# ZAI_GPS — README für Menschen

Dieses Dokument ist der **einzige menschliche Haupteinstieg** in das Repository.

Es enthält:
- den Zweck des Repos,
- die maßgeblichen Dokumente,
- die Regeln für die Zusammenarbeit mit KI-Agenten,
- den Arbeitsablauf für Menschen,
- die Normativitäts- und Konfliktregeln.

## 1. Zweck des Repos

`ZAI_GPS` ist die Firmware- und Integrationsbasis für ein AgOpenGPS-kompatibles Autosteer-/GNSS-System auf ESP32-S3-Basis.

Das Repo enthält:
- Firmware-Code,
- HAL-/Logic-Schichten,
- Board- und Bringup-spezifische Konfiguration,
- Backlog und Arbeitsaufträge,
- Architekturentscheidungen,
- Reports und Übergaben.

## 2. Maßgebliche Quellen und Rangfolge

Für die Arbeit im Repo gelten folgende Quellen in dieser Reihenfolge:

1. **Dieses `README.md`**
   - maßgeblich für Menschen,
   - maßgeblich für die Zusammenarbeit Mensch ↔ KI.

2. **`agents.md`**
   - maßgeblich für das Verhalten von KI-Agenten,
   - maßgeblich für Rollen, Inputs, Verbote und Übergaberegeln.

3. **ADRs unter `docs/adr/`**
   - maßgeblich für dauerhafte Architektur- und Designentscheidungen.

4. **Tasks im Backlog**
   - maßgeblich für die konkrete Arbeit an einer Änderung.

5. **Der aktuelle Repo-Zustand**
   - zeigt den Ist-Zustand, ersetzt aber keine normative Entscheidung.

6. **Handover- und Plan-Dokumente**
   - nur Kontextquellen,
   - bei Widerspruch zu README, `agents.md`, ADRs oder Tasks als **überholt** anzusehen.

## 3. Welche Dokumente wofür da sind

### `README.md`
Für Menschen:
- Einstieg,
- Arbeitsweise,
- Zusammenarbeit mit KI-Agenten,
- Repo-Regeln auf hoher Ebene.

### `agents.md`
Für KI-Agenten:
- Rollen,
- zulässige Inputs,
- Pflichtlektüre,
- Änderungsrechte,
- Eskalation,
- Regeln zur Überführung von Chatwissen in Repo-Artefakte.

### `docs/adr/`
Für Architekturentscheidungen:
- technische Invarianten,
- Dokumentenhierarchie,
- Config-Modell,
- Taskmodell,
- Modulsystem,
- Protokoll- und Subsystementscheidungen.

### `backlog/tasks/` und `backlog/index.yaml`
Für konkrete Arbeitspakete:
- Scope,
- Akzeptanzkriterien,
- Dependencies,
- Dateifootprints,
- Risiken,
- Owner.

### `reports/<TASK-ID>/`
Für Entwickler-Reports:
- Umsetzung,
- geänderte Dateien,
- Tests/Build,
- offene Punkte.

### Handover-/Plan-Dokumente
Nur als Kontext:
- historische Entscheidungen,
- Zwischenstände,
- Übergaben,
- alte Planungen.

Sie dürfen keine aktuell gültigen Regeln überstimmen.

## 4. Zusammenarbeit mit KI-Agenten

Menschen arbeiten mit KI-Agenten immer über **explizite Repo-Artefakte**, nicht über stillschweigend vorausgesetzten Chatkontext.

Das bedeutet:
- Wichtige Anforderungen gehören in einen Task.
- Dauerhafte Architekturentscheidungen gehören in einen ADR.
- Wichtige Erkenntnisse aus einer Implementierung gehören in den Entwickler-Report.
- Reine Chat-Erkenntnisse gelten als unzureichend konserviert, solange sie nicht im Repo materialisiert wurden.

### Grundregel
Ein späterer Agent oder Mensch muss eine Aufgabe **auch ohne Zugriff auf den ursprünglichen Chat** korrekt verstehen und fortführen können.

Wenn das nicht möglich ist, ist die Planung oder Dokumentation unvollständig.

## 5. Arbeitsablauf für Menschen

### 5.1 Einstieg
Vor Arbeit am Repo:
1. `README.md` lesen.
2. Relevante ADRs lesen.
3. Relevanten Task lesen.
4. Nur bei Bedarf Handover-/Plan-Dokumente als Kontext lesen.

### 5.2 Neue Arbeit starten
Wenn noch kein passender Task existiert:
- den **KI-Planer** beauftragen, einen Task anzulegen.

Menschen dürfen Tasks bei Bedarf auch selbst ergänzen oder korrigieren, aber die Standardlogik ist:
- Planung über Planer,
- Umsetzung über Entwickler,
- Prüfung über Reviewer.

### 5.3 Vor Implementierung
Vor jeder Umsetzung muss klar sein:
- welcher Task bearbeitet wird,
- welcher Branch genutzt wird,
- welche ADRs einschlägig sind,
- welche Dateien voraussichtlich betroffen sind.

### 5.4 Während der Arbeit
- kleine, nachvollziehbare Änderungen,
- Architekturgrenzen respektieren,
- keine impliziten Regeländerungen,
- keine stillen Abweichungen von ADRs oder Taskvorgaben.

### 5.5 Abschluss
Eine Arbeit ist erst vollständig, wenn:
- die Änderung umgesetzt ist,
- der Taskstatus gepflegt ist,
- ein Entwickler-Report vorhanden ist,
- offene Fragen benannt sind,
- Folgeentscheidungen bei Bedarf als ADR oder neuer Task konserviert wurden.

## 6. Branch- und Task-Regeln

### Grundsatz
Konkrete Implementierungsarbeit läuft auf einem **Task-Branch**:

`task/<TASK-ID>`

### Entwickler-Definition of Done
Ein Task gilt für Implementierungsarbeit nur dann als abgeschlossen, wenn:
- ein Entwickler-Report unter `reports/<TASK-ID>/<dev-name>.md` vorliegt,
- Änderungen und Testergebnisse dokumentiert sind,
- offene Punkte benannt sind oder explizit „keine“.

## 7. Regeln für Konflikte und Widersprüche

### 7.1 Dokumentkonflikte
Bei Widerspruch gilt:

`README.md` / `agents.md` / ADRs / Tasks  
**vor**  
Handover / Plänen / älteren Textresten

### 7.2 Code vs. Entscheidung
Wenn der Code einer gültigen ADR oder einem gültigen Task widerspricht:
- ist das kein stillschweigender Freibrief, der ADR/Task zu ignorieren,
- sondern ein Hinweis auf nötige Klärung oder Nacharbeit.

### 7.3 Chat vs. Repo
Wenn ein Chatinhalt wichtig ist, aber nicht im Repo steht:
- muss er in einen Task, ADR oder Report überführt werden,
- bevor er als stabiler Arbeitskontext gelten darf.

## 8. Wann ein ADR nötig ist

Ein ADR ist anzulegen, wenn mindestens eines davon zutrifft:
- die Entscheidung wirkt repo-weit,
- die Entscheidung ist architektonisch,
- die Entscheidung wurde diskutiert und ist nicht trivial,
- spätere Fehlinterpretation wäre teuer,
- mehrere Agenten oder Menschen werden darauf aufbauen.

Kein ADR nötig für:
- triviale Umbenennungen,
- rein lokale kosmetische Änderungen,
- einmalige Umsetzungsschritte ohne dauerhafte Architekturwirkung.

## 9. Spezialmodi

Spezielle Bringup- oder Diagnosepfade dürfen eigene Doku haben, aber:
- sie dürfen die globale Dokumenthierarchie nicht brechen,
- sie müssen auf die einschlägigen ADRs verweisen,
- sie dürfen globale Regeln nur mit expliziter dokumentierter Ausnahme einschränken.

## 10. Praktische Arbeitsregel

Wenn unklar ist, wo etwas hingehört, gilt:

- **menschliche Zusammenarbeit / Repo-Bedienung** → `README.md`
- **Agentenverhalten / Rollen / Inputs** → `agents.md`
- **dauerhafte technische Entscheidung** → ADR
- **konkreter Arbeitsauftrag** → Task
- **Umsetzungsrückblick** → Report
- **historischer Zwischenstand** → Handover

## 11. Migrationshinweis

Ältere Prozessdokumente, Handover-Texte und Plan-Dokumente können weiterhin existieren, gelten aber nur als Kontext, sofern ihre Inhalte nicht in diesem README, `agents.md`, ADRs oder Tasks bestätigt wurden.
