# TASK-013 HAL/Net-Interface-Claims und Parallelisierungsplan

- **ID**: TASK-013
- **Titel**: HAL-/Netzwerk-Schnittstellenänderungen mit expliziten Claims und Merge-Risikosteuerung planen
- **Status**: open
- **Priorität**: high
- **Komponenten**: `src/logic/*`, `src/hal/*`, `src/hal_esp32/*`, `docs/*`, `backlog/index.yaml`
- **Dependencies**: TASK-003
- **Kontext/Problem**:
  - Für die geplanten HAL-/Net-Änderungen fehlen pro Unteraufgabe standardisierte Datei-Footprints und klare Reihenfolgeregeln.
  - Ohne explizite Claims auf kritische Dateien (insbesondere Backlog-Index) steigt das Risiko für Merge-Konflikte.
- **Scope (in)**:
  - Subtask-Planung mit Pflichtfeldern gemäß `PLAN_AGENT`.
  - Explizite Lock-/Claim-Definition für erwartete Schreibpfade.
  - Abhängigkeitsklassifikation (`independent`/`dependent`) je Unteraufgabe.
- **Nicht-Scope (out)**:
  - Implementierung der eigentlichen Firmware-/HAL-Änderungen.
  - Funktionsvalidierung auf Zielhardware.
- **AC**:
  - Jede Unteraufgabe enthält: `change_intent`, `files_read`, `files_write`, `public_surface`, `risk_notes`, `classification`, `exclusive_before`, `parallelizable_after`.
  - `backlog/index.yaml` ist explizit als `merge_risk_file` mit exklusivem Claim im Planungs-/Backlog-Slot markiert.
  - Claims für alle erwarteten Schreibpfade sind eingeplant.
  - `dependent` ist gesetzt, sobald neue HAL/Public-Schnittstellen konsumiert werden.
- **Verifikation/Test**:
  - Markdown-Review der Claims/Abhängigkeiten.
  - Konsistenzcheck der referenzierten Pfade gegen Repository-Struktur.
- **Owner**: planning-team
- **Links**:
  - `docs/process/PLAN_AGENT.md`
  - `docs/process/QUICKSTART_WORKFLOW.md`
- **delivery_mode**: firmware_only

## Merge-Risk- und Claim-Rahmen

### merge_risk_files (explizit)
- `backlog/index.yaml` (**exklusiver Claim**, Slot: `planning/backlog`)
- `src/hal/hal.h`
- `src/logic/net.h`
- `src/logic/net.cpp`

### Claim-Slots
1. **Slot `planning/backlog` (exklusiv)**
   - Schreibzugriff: `backlog/index.yaml`
   - Zweck: Task-Metadaten/Planungsindex ohne Parallel-Edits sichern.
2. **Slot `hal-public-api` (exklusiv)**
   - Schreibzugriff: `src/hal/hal.h`
3. **Slot `logic-net` (koordiniert, bei Header-Änderung exklusiv)**
   - Schreibzugriff: `src/logic/net.cpp`, ggf. `src/logic/net.h`
4. **Slot `hal-esp32-impl` (koordiniert, bei Header-Änderung exklusiv)**
   - Schreibzugriff: `src/hal_esp32/hal_impl.cpp`, ggf. `src/hal_esp32/hal_impl.h`
5. **Slot `pgn-mapping` (optional, exklusiv während Schema-/Codec-Änderung)**
   - Schreibzugriff: `src/logic/pgn_types.h` oder `src/logic/pgn_codec.h`, `src/logic/pgn_codec.cpp`
6. **Slot `docs-handover` (parallel möglich, falls rein dokumentarisch)**
   - Schreibzugriff: `docs/*` + Session-Handover-Datei

## Unteraufgaben (Footprint-Blöcke)

### Subtask S1 — Planungs-/Backlog-Claim fixieren
- `change_intent`: Planungsmetadaten und Task-Index konsistent aktualisieren; exklusiven Backlog-Claim setzen.
- `files_read`:
  - `docs/process/PLAN_AGENT.md`
  - `backlog/README.md`
- `files_write`:
  - `backlog/index.yaml`
- `public_surface`:
  - Backlog-Taskliste und Planungsreihenfolge
- `risk_notes`:
  - Hohe Konfliktwahrscheinlichkeit bei parallelen Task-Index-Änderungen; daher exklusiver Slot Pflicht.
- `classification`: dependent
- `exclusive_before`: []
- `parallelizable_after`: []

### Subtask S2 — HAL-Public-Interface definieren/frieren
- `change_intent`: Neue HAL-Schnittstellen in `hal.h` definieren und stabilisieren.
- `files_read`:
  - `src/hal/hal.h`
  - `src/logic/net.h`
  - `src/logic/net.cpp`
- `files_write`:
  - `src/hal/hal.h`
- `public_surface`:
  - Öffentliche HAL-API (Funktionssignaturen, Datentypen)
- `risk_notes`:
  - API-Brüche propagieren in `logic` und `hal_esp32`; API-Slot exklusiv halten.
- `classification`: dependent
- `exclusive_before`:
  - S1
- `parallelizable_after`: []

### Subtask S3 — ESP32-HAL-Implementierung auf neue API anpassen
- `change_intent`: HAL-Implementierung gegen aktualisierte `hal.h` anbinden.
- `files_read`:
  - `src/hal/hal.h`
  - `src/hal_esp32/hal_impl.cpp`
  - `src/hal_esp32/hal_impl.h`
- `files_write`:
  - `src/hal_esp32/hal_impl.cpp`
  - `src/hal_esp32/hal_impl.h` (falls benötigt)
- `public_surface`:
  - Keine neue Public API; Implementierungsvertrag zur HAL-API
- `risk_notes`:
  - Drift zwischen Header und Implementierung möglich; mit S2 synchron halten.
- `classification`: dependent
- `exclusive_before`:
  - S2
- `parallelizable_after`: []

### Subtask S4 — Net-Logik an HAL-Schnittstelle koppeln
- `change_intent`: `net.cpp` (und ggf. `net.h`) auf neue HAL/Public-Verträge umstellen.
- `files_read`:
  - `src/logic/net.cpp`
  - `src/logic/net.h`
  - `src/hal/hal.h`
- `files_write`:
  - `src/logic/net.cpp`
  - `src/logic/net.h` (falls benötigt)
- `public_surface`:
  - `src/logic/net.h` (falls Signaturänderungen), Net↔HAL-Aufrufvertrag
- `risk_notes`:
  - Bei Headeränderungen erhöhte Merge-Gefahr; `logic-net`-Slot dann exklusiv führen.
- `classification`: dependent
- `exclusive_before`:
  - S2
- `parallelizable_after`:
  - S3

### Subtask S5 — Optionales PGN-Mapping/Skalierung anpassen
- `change_intent`: Mapping-/Skalierungslogik nur bei Bedarf auf neue Net/HAL-Datenpfade abstimmen.
- `files_read`:
  - `src/logic/pgn_types.h`
  - `src/logic/pgn_codec.h`
  - `src/logic/pgn_codec.cpp`
  - `src/logic/net.cpp`
- `files_write`:
  - `src/logic/pgn_types.h` **oder** `src/logic/pgn_codec.h`
  - `src/logic/pgn_codec.cpp` (falls Codec-Pfad betroffen)
- `public_surface`:
  - PGN-Datentypen/Codec-Vertrag (nur bei tatsächlicher Anpassung)
- `risk_notes`:
  - Semantische Regressionen bei Skalierungsfaktoren; Testpflicht für Discovery/Core-Pfade.
- `classification`: dependent
- `exclusive_before`:
  - S4
- `parallelizable_after`: []

### Subtask S6 — Dokumentation & Session-Handover
- `change_intent`: Änderungspfad, Claims und Ergebnisse in `docs/` und Session-Handover dokumentieren.
- `files_read`:
  - `docs/process/PLAN_AGENT.md`
  - `templates/session-handover.md`
- `files_write`:
  - `docs/` (betroffene Plan-/Prozessdoku)
  - Session-Handover-Datei unter `docs/`
- `public_surface`:
  - Projektdokumentation/Arbeitsübergabe
- `risk_notes`:
  - Geringes Implementierungsrisiko; Konflikte primär bei gleichzeitigen Doku-Edits.
- `classification`: independent
- `exclusive_before`: []
- `parallelizable_after`:
  - S1
