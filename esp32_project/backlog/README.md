# Backlog

Dieses Verzeichnis enthält den projektweiten Arbeitsvorrat aus `docs/Handover2.md` in strukturierter Form.

## Struktur

- `backlog/tasks/`: einzelne Aufgaben als eigene Markdown-Dateien.
- `backlog/epics/`: thematische Sammelobjekte zur Gruppierung mehrerer Tasks.
- `backlog/index.yaml`: zentraler Index mit Enums sowie Referenzen auf alle Epics und Tasks.

## Pflichtfelder für jede Task-Datei

Jede Datei in `backlog/tasks/` muss folgende Felder enthalten:

- **ID**: Eindeutige Kennung, Format `TASK-XXX`.
- **Titel**: Kurzbeschreibung der Aufgabe.
- **Status**: Einer von `open`, `in_progress`, `blocked`, `done`.
- **Priorität**: Einer von `high`, `medium`, `low`.
- **Komponenten**: Betroffene Module/Dateibereiche.
- **Dependencies**: Liste von Task-IDs oder `none`.
- **AC**: Akzeptanzkriterien als prüfbare Bullet-Liste.
- **Owner**: Verantwortliche Rolle oder Person.
- **Links**: Verweise auf relevante Dokumente/Dateien.
- **delivery_mode**: Art der Durchführung:
  - `hardware_required`: Reale Hardware/Flash/Test nötig.
  - `firmware_only`: Reine Firmware-/Code-Änderung.
  - `mixed`: Kombination aus Firmware und Hardwarevalidierung.

## Rollen-Regel

- Tasks werden **ausschließlich** vom KI-Planer angelegt. KI-Entwickler dürfen keine neuen Task-Dateien erstellen und den `backlog/index.yaml` nicht erweitern.
- Initial trägt sich der KI-Planer als `Owner` ein; KI-Entwickler werden im Feld `Owner` erst nach expliziter Zuordnung eingetragen.

## Konventionen

- Eine Task-Datei pro offener Aufgabe.
- Dateiname: `<ID>-<kebab-case-titel>.md`.
- Epics referenzieren Tasks per ID, Tasks referenzieren ihr Epic in den Links.



- Prozess-/Hygiene-Nacharbeit: Ergänze im Task-Titel oder Kontext den Marker `[#process-hygiene]` für alle Nacharbeits-Tasks aus Prozessabweichungen.
  - Zweck: Sichtbarkeit im Index und einfache Nachverfolgung über Suche/Filter.
  - Gilt insbesondere für Trigger wie falscher Branch, fehlender Report oder Rollenverstoß.


## Task-Abschluss

- Nach Merge einer Task setzt der KI-Planer den Status in **Task-Datei und `backlog/index.yaml` synchron** auf `done`.
- Bei abgeschlossenen Task-Ketten (z. B. compile-time -> boot-init -> pin-zuweisung) sind die abhängigen Tasks in korrekter Reihenfolge zu schließen.

## Validierung

- Optionaler Konsistenzcheck: `python3 tools/validate_backlog_index.py` (benötigt `PyYAML`).
- Reviewer/Planer schließen einen Task nur mit vorhandenem Report-Artefakt unter `reports/<Task-ID>/<dev-name>.md` (gemäß `templates/dev-report.md`).
