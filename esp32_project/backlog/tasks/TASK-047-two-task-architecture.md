# TASK-047: Two-Task Architecture — task_fast + task_slow

- **ID**: TASK-047
- **Status**: open
- **Priority**: high
- **Epic**: EPIC-001 (Runtime Stability)
- **Delivery Mode**: firmware_only
- **Base Branch**: `main`
- **Working Branch**: `task/two-task-architecture`
- **Owner**: KI-Planer

## Origin

Chat-Diskussion (2025-06-18): Nutzer forderte Vereinfachung des Repo auf zwei Tasks.
Motivation: Aktuelle Architektur mit 3 Tasks + Arduino loop() ist zu komplex
und inkonsistent (commTask macht fast nichts, maintTask wird teilweise
von main.cpp, teilweise vom LOGGING-Modul gestartet).

## Diskussion

Zwei Kernforderungen aus dem Chat:

1. **task_fast**: Fixer Prozessor (Core 1), zuverlässig bei konfigurierbarer
   Frequenz (default 100 Hz), folgt input → process → output Pipeline.
2. **task_slow**: Anderer Prozessor (Core 0), keine fixe Frequenz,
   verwaltet alle langsamen Prozesse (HW-Monitoring, CLI, DBG, Sub-Tasks).

Zusätzliche Anforderungen, die im Chat diskutiert und akzeptiert wurden:
- Arduino `loop()` nur zum Starten der beiden Tasks (danach leer).
- Sub-Tasks (NTRIP, SD) werden von task_slow gestartet und an Core 0 gebunden.
- Datenübergabe task_slow → task_fast ausschließlich über globale Variablen
  mit Metadaten (Aktualität, dirty_flag) unter StateLock (ADR-STATE-001).
- Betriebsmodi vereinfacht auf SETUP und RUN (ersetzt ADR-005).

## Scope

### In Scope
- Ersetzen von controlTask + commTask + maintTask durch task_fast + task_slow
- Shared State Mechanismus (SharedSlot<T> Template)
- Frequenz-Konfiguration per PlatformIO-Profil
- Betriebsmodus SETUP/RUN (Konfiguration vs. Betrieb)
- Sub-Task Lifecycle-Management in task_slow
- Migration von HW-Status-Monitoring aus commTask in task_slow
- Migration von NTRIP/SD-Background-Work aus maintTask in task_slow
- Arduino loop() bereinigen (leer nach Task-Start)

### Out of Scope
- Umbenennung von OpMode::CONFIG/WORK zu SETUP/RUN (Follow-Up)
- ADR-005 WiFi/BT-Integration im SETUP-Modus (Follow-Up)
- Neue Sub-Tasks für zukünftige Features
- Umstellung aller Module auf den neuen Shared State (schrittweise Migration)

## Akzeptanzkriterien

### AC-1: Zwei-Tasks starten korrekt
- [ ] `setup()` erstellt genau zwei FreeRTOS-Tasks (task_fast, task_slow)
- [ ] task_fast läuft auf Core 1 mit konfigurierbarer Frequenz (default 100 Hz)
- [ ] task_slow läuft auf Core 0 mit variabler Frequenz
- [ ] Arduino `loop()` enthält keine eigene Logik mehr

### AC-2: Modul-Pipeline funktioniert
- [ ] task_fast ruft `moduleSysRunInput/Process/Output()` im RUN-Modus auf
- [ ] task_fast ist angehalten im SETUP-Modus (nur WDT-Feed)
- [ ] Pipeline-Frequenz ist messbar und stabil (Jitter < 1 ms)

### AC-3: task_slow übernimmt alle Hintergrundarbeiten
- [ ] HW-Status-Monitoring (~1 Hz) funktioniert wie bisher
- [ ] DBG.loop() wird regelmäßig aufgerufen (TCP-Verbindung funktioniert)
- [ ] Serial/TCP CLI-Polling funktioniert
- [ ] WDT wird gefüttert

### AC-4: Shared State Mechanismus
- [ ] `SharedSlot<T>` Template implementiert in `shared_state.h`
- [ ] Alle Cross-Task-Datenflüsse verwenden SharedSlot + StateLock
- [ ] task_fast kann dirty/stale-Zustand prüfen und Daten verwerfen

### AC-5: Betriebsmodi
- [ ] SETUP-Modus: task_slow voll aktiv, task_fast angehalten
- [ ] RUN-Modus: task_fast Pipeline, task_slow reduziert
- [ ] Übergang SETUP → RUN: Safety-Pin HIGH oder CLI `mode run`
- [ ] Übergang RUN → SETUP: Safety-Pin LOW + Speed < Schwellwert oder CLI `mode setup`

### AC-6: Sub-Tasks
- [ ] NTRIP-Background-Work läuft als Sub-Task unter task_slow
- [ ] SD-Flush läuft als Sub-Task unter task_slow
- [ ] Sub-Tasks sind an Core 0 gebunden
- [ ] Sub-Tasks kommunizieren über SharedSlot (kein direkter task_fast-Zugriff)

### AC-7: Konfiguration
- [ ] `TASK_FAST_HZ` per PlatformIO-Profil setzbar
- [ ] `TASK_FAST_STACK` per PlatformIO-Profil setzbar
- [ ] Build mit `TASK_FAST_HZ=100` erfolgreich
- [ ] Build mit `TASK_FAST_HZ=200` erfolgreich

## Dateifootprint

### Neu
- `src/logic/shared_state.h` — SharedSlot<T> Template
- `docs/adr/ADR-007-two-task-architecture.md`

### Geändert
- `src/main.cpp` — setup() Task-Erstellung, loop() bereinigen, controlTaskFunc/commTaskFunc → task_fast/task_slow
- `platformio.ini` — TASK_FAST_HZ, TASK_FAST_STACK Build-Flags

### Potenziell geändert (Migration)
- `src/logic/mod_ntrip.cpp` — Sub-Task Lifecycle an task_slow anpassen
- `src/logic/mod_logging.cpp` — maintTask → Sub-Task unter task_slow

## Invarianten

1. **task_fast blockiert niemals.** Kein TCP, kein SD, kein Serial-Read.
2. **Alle Cross-Task-Daten unter StateLock.** (ADR-STATE-001)
3. **task_slow ist alleiniger Lifecycle-Owner für Sub-Tasks.**
4. **Watchdog in beiden Modi aktiv.**
5. **Modulsystem (ModuleOps2) unverändert.** Module merken nichts von der Task-Umstellung.
6. **Core 1 gehört exklusiv task_fast.** Kein anderer Task/Interrupt darf Core 1 nutzen.

## Known Traps

- **maintTask wird teilweise vom LOGGING-Modul erstellt** (`sdLoggerMaintInit()`).
  Die Migration muss sicherstellen, dass der LOGGING-Sub-Task sauber an task_slow
  übergeben wird.
- **NTRIP blocking TCP connect (5 s Timeout)** muss als Sub-Task laufen,
  nicht inline in task_slow (würde HW-Monitoring und CLI blockieren).
- **StateLock im task_fast Hotpath** — jede Lock-Operation kostet ~1-5 µs.
  Bei 100 Hz Pipeline und 5 geschützten Reads → ~25 µs Overhead pro Zyklus
  → vernachlässigbar bei 10 ms Zykluszeit.
- **shared_state.h muss vor allen Modulen inkludiert werden** die es nutzen.
  Abhängigkeitsreihenfolge im Build beachten.

## Rejected Alternativen

- **Arduino loop() als task_slow nutzen**: loop() hat IDLE-Priorität, keinen
  eigenen Stack, kein sauberen Lifecycle. Verhungert bei Sub-Task-I/O.
- **Alles asynchron mit State-Machines (keine Sub-Tasks)**: NTRIP TCP connect
  und SD flush als State-Machine umzuschreiben ist hoher Aufwand und fehleranfällig.
  Pragmatischer: bestehende blocking I/O in Sub-Tasks belassen.

## Dependencies

- ADR-002 (Taskmodell) — wird ersetzt
- ADR-005 (Betriebsmodus) — wird vereinfacht
- ADR-STATE-001 (StateLock) — weiterhin gültig, Grundlage für Shared State
- ADR-MODULE-002 (Modulsystem) — unverändert

## Referenzen

- `docs/adr/ADR-007-two-task-architecture.md` — Architekturentscheidung
- `docs/adr/ADR-002-task-model-control-comm-maint.md` — Aktuelles Task-Modell (wird ersetzt)
- `docs/adr/ADR-005-operating-mode-system.md` — Aktuelle Betriebsmodi (wird vereinfacht)
- `docs/adr/ADR-STATE-001-strict-statelock-for-shared-state.md` — StateLock-Regeln

## Parallelizable After

Keine — diese Task ist ein architektonischer Umbau der zentralen Task-Struktur.
Alle nachfolgenden Tasks sollten auf der neuen Architektur aufbauen.
