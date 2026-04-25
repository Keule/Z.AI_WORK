# ADR-007: Two-Task Architecture (task_fast + task_slow)

- Status: accepted
- Datum: 2025-06-18
- Supersedes: ADR-002, ADR-005
- Verwandte ADRs: ADR-002 (Taskmodell — **ersetzt**), ADR-005 (Betriebsmodus — **vereinfacht**), ADR-STATE-001 (StateLock — **weiterhin gültig**), ADR-MODULE-002 (Modulsystem — **weiterhin gültig**)

## Kontext

Die Firmware hat aktuell drei FreeRTOS-Tasks (controlTask, commTask, maintTask)
plus den Arduino `loop()`. Diese Struktur ist über die Zeit gewachsen und hat
mehrere Probleme:

1. **commTask macht fast nichts** — nur 1 Hz HW-Status-Monitoring mit 100 Hz
   Poll-Rate. 99 % der Zyklen sind Leerlauf.
2. **maintTask wird inkonsistent gestartet** — mal vom LOGGING-Modul, mal von
   main.cpp, je nachdem ob SD-Karte oder NTRIP aktiv ist.
3. **Drei Tasks + loop() = vier Ausführungskontexte** auf zwei Kernen —
   unnötig komplex, schwer zu debuggen, viele Cross-Task-Koordinationspunkte.
4. **ADR-005 (ACTIVE/PAUSED)** ist ambitioniert aber noch nicht implementiert.
   BOOTING+PAUSED+ACTIVE mit WiFi/BT-Steuerung ist über engineered für den
   aktuellen Stand.

### Ziel

- **Zwei Tasks** als einzige Ausführungskontexte (abgesehen von setup()).
- Klare Trennung: Echtzeit-Pipeline vs. alles andere.
- Einheitlicher Lifecycle für alle langsamen Operationen.
- Zwei Betriebsmodi: **SETUP** und **RUN** (vereinfacht aus ADR-005).

## Entscheidung

### 1. Betriebsmodi

Zwei Modi, ersetzen BOOTING/ACTIVE/PAUSED aus ADR-005:

| Modus | task_fast | task_slow | Beschreibung |
|-------|-----------|-----------|-------------|
| **SETUP** | Angehalten (WDT-Feed) | Voll aktiv | Konfiguration, Diagnose, CLI, Module aktivieren/deaktivieren |
| **RUN** | Pipeline @ Freq Hz | Reduziert | Autosteer-Betrieb, Echtzeit-Pipeline |

**Übergänge:**
- `SETUP → RUN`: Safety-Pin HIGH nach Boot, oder CLI `mode run`
- `RUN → SETUP`: Safety-Pin LOW UND Geschwindigkeit < Schwellwert, oder CLI `mode setup`

**Konsistenz mit ADR-005:** ADR-005 definiert drei Zustände (BOOTING, ACTIVE, PAUSED).
ADR-007 vereinfacht auf zwei (SETUP, RUN). BOOTING fällt als Initialisierungsphase in
`setup()` (kein eigener Modus). PAUSED wird zu SETUP zusammengefasst — die
Unterscheidung "Boot-Konfiguration" vs. "Laufzeit-Konfiguration" erfordert keine
getrennten Modi.

### 2. Tasks

#### task_fast (Core 1 — fester Kern)

- **Priorität**: Hoch (`configMAX_PRIORITIES - 2`)
- **Frequenz**: Konfigurierbar per PlatformIO-Profil (Default: 100 Hz)
- **Stack**: 4096 Bytes
- **Inhalt**:
  - Modul-Pipeline: `moduleSysRunInput()` → `moduleSysRunProcess()` → `moduleSysRunOutput()`
  - Modus-Prüfung: nur in RUN aktiv
  - Pipeline-Diagnostik (optionale Hz-Ausgabe)
- **Invarianten**:
  - Keine blockierenden Operationen
  - Kein direkter HW-Zugriff außer über HAL
  - Kein Serial/DBG-Output im Hotpath (nur über geplante Diag-Punkte)
  - Shared-State-Zugriff nur unter StateLock (ADR-STATE-001)

#### task_slow (Core 0 — variabel)

- **Priorität**: Mittel (`configMAX_PRIORITIES - 4`)
- **Frequenz**: Variabel, gesteuert durch `vTaskDelay()` und Events
- **Stack**: 8192 Bytes (größer wegen CLI/Config-Menü-Stack-Bedarf)
- **Inhalt (RUN-Modus)**:
  - HW-Status-Monitoring (~1 Hz)
  - DBG.loop() für TCP
  - Serial/TCP CLI-Polling
  - WDT-Feed
  - Sub-Task-Management
- **Inhalt (SETUP-Modus)**:
  - Alles aus RUN-Modus, plus:
  - Vollzugriff CLI (alle Kommandos)
  - Setup-Wizard
  - Modul activate/deactivate
  - Konfiguration ändern
  - Diagnose-Ausgaben
- **Invarianten**:
  - Darf blockieren (TCP connect, SD write)
  - Verwaltet Lifecycle aller Sub-Tasks
  - Shared-State-Zugriff unter StateLock

#### Arduino loop()

- Bleibt bestehen, dient aber nur als **Setup-Orchestrator**.
- Nach dem Start der beiden Tasks ist `loop()` leer bis auf optionalen WDT-Feed.
- Kann bei Bedarf vollständig deaktiviert werden (endloser `vTaskDelay()`).

### 3. Sub-Tasks (Feature-spezifische Worker)

Module die lang laufende I/O-Operationen benötigen (TCP connect, SD flush)
können **einen** dedizierten Sub-Task auf Core 0 spawnen.

**Regeln für Sub-Tasks:**
- Maximal ein Sub-Task pro Modul.
- Sub-Tasks werden an Core 0 gepinnt.
- Sub-Tasks werden von `task_slow` gestartet und beendet (Lifecycle-Owner).
- Sub-Tasks kommunizieren mit `task_fast` **ausschließlich** über Shared State
  (StateLock-geschützt, mit Timestamp + Dirty-Flag).
- Sub-Tasks dürfen `task_fast` niemals direkt aufrufen oder blockieren.

**Bestehende Sub-Tasks (Migration):**
- NTRIP: TCP connect/reconnect → schreibt RTCM-Daten in Shared Buffer
- SD Logger: SD flush → geplante Auslagerung aus maintTask

### 4. Shared State Pattern (task_slow ↔ task_fast)

Alle Cross-Task-Daten fließen über einen einheitlichen Mechanismus:

```cpp
/// Template für einen Shared Data Slot
/// Jeder Slot repräsentiert einen Datenkanal von task_slow/Sub-Task
/// zu task_fast.
template<typename T>
struct SharedSlot {
    T        data {};               // Die eigentlichen Nutzdaten
    uint32_t last_update_ms = 0;    // Timestamp des letzten Schreibens
    bool     dirty = false;         // true = neue Daten seit letztem Lesen
    uint8_t  source_id = 0;         // Welcher Schreiber (Modul-ID)
};

/// Verbraucht von task_fast (in ModuleResult input):
///   StateLock lock;
///   if (slot.dirty && (now_ms - slot.last_update_ms < FRESHNESS_MS)) {
///       // Daten verarbeiten
///       slot.dirty = false;
///   }
///   // Andernfalls: Daten veraltet → ModuleResult { false, ERR_STALE }
```

**Konsistenz mit ADR-STATE-001:** Alle Shared-Slot-Zugriffe erfolgen unter
`StateLock`. Dies ist nicht verhandelbar.

### 5. Frequenz-Konfiguration

Die task_fast-Frequenz wird per PlatformIO-Profil konfiguriert:

```ini
[env:profile_full_steer_ntrip]
build_flags =
    ...
    -DTASK_FAST_HZ=100       ; Default: 100 Hz
    -DTASK_FAST_STACK=4096    ; Stack-Größe (Bytes)
```

In `main.cpp`:
```cpp
#ifndef TASK_FAST_HZ
#define TASK_FAST_HZ 100
#endif
static constexpr uint32_t TASK_FAST_INTERVAL_MS = 1000 / TASK_FAST_HZ;
```

### 6. Modus-Abfrage in Modulen

Module die sich unterschiedlich in SETUP vs. RUN verhalten, können den Modus
über ein Makro oder Funktionsaufruf prüfen:

```cpp
// In mod_*.cpp output() oder is_healthy():
if (modeGet() == OpMode::CONFIG) {
    // SETUP-Verhalten (z.B. DBG.loop() aufrufen)
} else {
    // RUN-Verhalten (Pipeline)
}
```

**Hinweis:** Der bestehende `OpMode`-Enum (`CONFIG`/`WORK`) aus
`module_interface.h` wird **nicht geändert**. `CONFIG` ≡ SETUP, `WORK` ≡ RUN.
Dies vermeidet Breaking Changes im Modulsystem. Eine spätere Umbenennung
(CONFIG → SETUP, WORK → RUN) ist als Follow-Up Task möglich.

### 7. Datei-Layout

```
src/main.cpp
  └── setup()
       ├── Phase 1-6: Hardware + Module Init
       └── Phase 7:
            ├── xTaskCreatePinnedToCore(task_fast, Core 1)
            └── xTaskCreatePinnedToCore(task_slow, Core 0)
  └── loop()
       └── (leer oder vTaskDelay(portMAX_DELAY))

src/logic/task_fast.h / task_fast.cpp       (neu, optional — kann in main.cpp bleiben)
src/logic/task_slow.h / task_slow.cpp       (neu, optional — kann in main.cpp bleiben)
src/logic/shared_state.h                    (neu — SharedSlot<T> Template)
```

## Invarianten

1. **task_fast darf niemals blockieren.** Kein TCP, kein SD, kein Serial-Read.
2. **task_fast greift nur über StateLock auf Shared State zu.** (ADR-STATE-001)
3. **task_slow ist der alleinige Lifecycle-Owner für Sub-Tasks.**
4. **Watchdog ist in beiden Modi aktiv.** (task_slow füttert TWDT.)
5. **Safety-Schaltung ist immer aktiv.**
6. **Modulsystem bleibt unverändert.** Module sehen keine Task-Änderung —
   sie implementieren weiterhin input()/process()/output().
7. **Arduino loop() darf keine eigene Logik mehr enthalten.**
8. **Sub-Tasks sind an Core 0 gebunden.** Core 1 gehört exklusiv task_fast.

## Migration von ADR-002

| ADR-002 | ADR-007 |
|---------|---------|
| controlTask (Core 1, 200 Hz) | task_fast (Core 1, konfigurierbar, default 100 Hz) |
| commTask (Core 0, 100 Hz) | **entfällt** → Aufgabe wandert in task_slow |
| maintTask (Core 0, variabel) | **entfällt** → Aufgabe wandert in task_slow |
| Arduino loop() (Core 0, ~10 Hz) | **entfällt** → bleibt nur als setup()-Orchestrator |

## Konsequenzen

### Positiv
- Reduzierung von 4 Ausführungskontexten auf 2 (plus setup)
- Klare Verantwortlichkeiten: Echtzeit vs. Hintergrund
- Einfacheres Debugging (nur 2 Tasks im Trace)
- Konsistenter Lifecycle (Sub-Task-Management zentral)
- Besser testbar (task_fast ist deterministisch, task_slow ist isoliert)

### Negativ
- Einmaliger Migrationsaufwand (commTask/maintTask → task_slow)
- Sub-Tasks brauchen saubere Shared-State-Kopplung (ADR-STATE-001)
- ADR-002 und Teile von ADR-005 werden obsolet

### Risiken
- task_slow wird zum Single-Point-of-Failure für alle Hintergrundarbeiten
  → Mitgedacht: wenn task_slow abstürzt, füttert er nicht mehr TWDT → Reset
- 100 Hz statt 200 Hz für Pipeline
  → AgOpenGPS sendet bei ~10 Hz, PID braucht ~50 Hz — 100 Hz ist ausreichend

## Abgelehnte Alternativen

- **Drei Tasks beibehalten (controlTask + commTask + maintTask)**
  → commTask tut zu wenig, Aufwand nicht gerechtfertigt.

- **Alles in einem Task (cooperative multitasking in loop())**
  → Blockierende I/O verhungert den IDLE-Task → WDT-Reset.

- **Arduino loop() als task_slow nutzen**
  → loop() hat IDLE-Priorität, kein eigener Stack, kein sauberer Lifecycle.
  Wenn ein Sub-Task blocking I/O macht, verhungert loop().

- ** Mehr als zwei Tasks (z.B. separater CLI-Task, separater Diag-Task)**
  → Über engineered, zu viele Kontextwechsel auf zwei Kernen.

## Plan-Integration

Dieser ADR ist Grundlage für TASK-047. Nach Akzeptanz:
1. ADR-002 als `superseded` markieren
2. ADR-005 als `superseded` markieren (SETUP/RUN ersetzt ACTIVE/PAUSED/BOOTING)
3. TASK-047 implementiert die Migration
