# ADR-LOG-002: Tag-Registry und Thread-sichere Runtime-Filter

- Status: accepted
- Datum: 2026-04-23

## Kontext

`log all <level>` und `log status` basieren auf `kAllTags[]` in `src/logic/log_ext.cpp`.
Damit diese Kommandos verlässlich sind, muss die Tag-Liste vollständig sein.

Gleichzeitig werden `log_filter_line` und `log_filter_file` von mehreren Tasks genutzt
(`controlTask`, `commTask`, `maintTask`) und zur Laufzeit per Serial-Kommando geändert.
Aktuell gibt es keine formale Synchronisationsregel für konsistente parallele Zugriffe.

## Entscheidung

1. **Tag-Registry als verbindliche Vollständigkeitsliste**
   - `kAllTags[]` ist die maßgebliche Registry aller produktiv genutzten Logging-Tags.
   - Bei Einführung eines neuen Tags ist die Aktualisierung von `kAllTags[]` **obligatorisch**
     (inkl. Reihenfolge-/Namenkonsistenz zum verwendeten `ESP_LOG*`-Tag).
   - Änderungen an Logging-Tags ohne gleichzeitige Aktualisierung von `kAllTags[]` gelten
     als unvollständig und dürfen nicht als "done" betrachtet werden.

2. **Synchronisationsstrategie für Runtime-Filter**
   - Für den gemeinsamen Zustand aus `log_filter_line` und `log_filter_file` gilt eine
     **atomare Publish/Read-Regel** (Snapshot-Regel):
     - Writer publiziert immer einen vollständigen, in sich konsistenten Snapshot.
     - Reader bewertet nur einen vollständig gelesenen Snapshot.
   - Die Regel kann technisch über einen Mutex **oder** lock-freie Sequenz-/Generationslogik
     umgesetzt werden; entscheidend ist die atomare Sicht auf das Paar aus Datei+Zeile.
   - Teilzustände (z. B. neue Datei mit alter Zeile) sind nicht zulässig.

## Invarianten

- `kAllTags[]` bildet alle zur Laufzeit verwendeten Tags vollständig ab.
- `log all` und `log status` arbeiten stets auf derselben Tag-Registry (`kAllTags[]`).
- Runtime-Filter-Lesezugriffe aus beliebigen Tasks sehen nur konsistente Snapshots.

## Akzeptanzkriterien

- Bei parallelem Logging aus `controlTask`, `commTask` und `maintTask` bleibt das
  Filterverhalten konsistent:
  - `filter off` deaktiviert den Filter deterministisch für alle drei Tasks.
  - `filter <file>` aktiviert deterministisch Dateifilter für alle drei Tasks.
  - `filter <file>:<line>` aktiviert deterministisch Datei+Zeilenfilter für alle drei Tasks.
- Während häufiger Filterwechsel (Serial-Kommandos) treten keine Mischzustände auf
  (kein Flackern zwischen alter/neuer Datei-Zeile-Kombination).
- Neue Logging-Tags sind erst dann akzeptiert, wenn `kAllTags[]` im selben Change-Set
  erweitert wurde.

## Konsequenzen

### Positiv
- Verlässliche Runtime-Logsteuerung (`log all`, `log status`).
- Definierte Thread-Sicherheit für Filterzustand.
- Klarer Review-Maßstab bei Änderungen am Logging.

### Negativ
- Etwas zusätzlicher Pflegeaufwand bei neuen Tags.
- Implementierung der Snapshot-Regel kann zusätzliche Synchronisationskosten erzeugen.

## Alternativen

- **Keine zentrale Tag-Registry**
  - verworfen, da `log all`/`log status` sonst unvollständig oder inkonsistent werden.

- **Unsynchronisierte globale Filtervariablen**
  - verworfen, da parallele Zugriffe zu inkonsistenten Zwischenzuständen führen können.

## Plan-Integration

Wird in Schritt 5 (Basis-Konfig-Framework) bei der System-Config-Kategorie umgesetzt.

