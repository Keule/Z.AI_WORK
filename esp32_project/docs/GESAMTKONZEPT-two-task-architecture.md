# Gesamtkonzept: 2-Task-Architektur (task_fast + task_slow)

**Datum**: 2025-06-18
**Status**: Entwurf — zur Diskussion
**Basis**: ADR-007 (akzeptiert), TASK-047 (open)
**Branch**: `task/two-task-architecture` (ab `main`)

---

## 1. Motivation & Ist-Zustand

### 1.1 Probleme der aktuellen Architektur

Die Firmware hat aktuell **vier Ausführungskontexte** auf zwei Kernen:

| Kontext | Kern | Rate | Problem |
|---------|------|------|---------|
| `controlTask` | Core 1 | 200 Hz | Funktioniert, aber feste Rate |
| `commTask` | Core 0 | 100 Hz | 99 % Leerlauf (nur 1 Hz HW-Status) |
| `maintTask` | Core 0 | variabel | Inkonsistenter Startort (mal main.cpp, mal LOGGING-Modul) |
| `loop()` | Core 0 | ~10 Hz | DBG.loop(), CLI-Polling, Setup-Wizard, WDT |

**Kernprobleme:**

1. **commTask verschwendet Ressourcen** — 100 Hz Poll-Rate für 1 Hz Monitoring. Ein Task-Context-Wechsel kostet ~1-5 µs auf ESP32-S3 → 95-99 µs Overhead pro Sekunde nur für Kontextwechsel, ohne dass produktive Arbeit geleistet wird.

2. **maintTask wird inkonsistent gestartet** — `sdLoggerMaintInit()` wird aufgerufen von:
   - `mod_logging.cpp::activate()` wenn SD-Karte erkannt
   - `main.cpp::bootStartTasks()` wenn NTRIP aktiv aber keine SD
   - Das führt zu unklarem Ownership und schwer auffindbaren Bugs.

3. **Vier Kontexte, zwei Kerne** — Zu viele Ausführungskontexte erschweren Debugging, Trace-Analyse und mentales Modell. Jeder zusätzliche Task bedeutet: eigenen Stack, eigenen Watchdog-Subscription, eigene Error-Handling-Pfade.

4. **ADR-005 (ACTIVE/PAUSED/BOOTING) nie vollständig implementiert** — Die 3-Zustands-Maschine mit GPIO-Toggle, NVS-Persistenz und AgIO-Notification wurde in `op_mode.cpp` implementiert, aber `main.cpp` nutzt bereits das einfachere CONFIG/WORK-Modell aus dem Modulsystem. Es gibt **zwei konkurrierende OpMode-Systeme**:
   - `op_mode.h`: `enum OpMode { BOOTING, ACTIVE, PAUSED }` (C-Style, mit Mutex + Atomic)
   - `module_interface.h`: `enum class OpMode { CONFIG, WORK }` (C++-Style, einfacher)

### 1.2 Zielbild

**Zwei Tasks. Ein System. Klare Verantwortlichkeiten.**

```
Core 1:  task_fast  ─── Pipeline @ konfigurierbarer Hz (default 100 Hz)
                       │   input() → process() → output()
                       │   Nie blockiert. Kein I/O.
                       │
Core 0:  task_slow  ─── Alles andere
                       │   HW-Monitoring, DBG.loop(), CLI, WDT
                       │   Darf blockieren (TCP, SD, Serial)
                       │   Lifecycle-Owner für Sub-Tasks
                       │
Sub-Tasks (Core 0):    NTRIP-Worker, SD-Flush-Worker
                       └── gestartet/gestoppt von task_slow
```

---

## 2. Architektur

### 2.1 task_fast — Echtzeit-Pipeline

**Zweck:** Feste Rate, deterministisch, nie blockierend. Führt die Modul-Pipeline aus.

| Eigenschaft | Wert |
|-------------|------|
| Kern | Core 1 (exklusiv) |
| Priorität | `configMAX_PRIORITIES - 2` (hoch) |
| Frequenz | Konfigurierbar per PlatformIO, default 100 Hz |
| Stack | 4096 Bytes |
| Blocking | **Nie** — kein TCP, kein SD, kein Serial-Read, kein `delay()` |

**Ablauf pro Zyklus:**

```cpp
void taskFastFunc(void*) {
    vTaskDelay(pdMS_TO_TICKS(500));  // Stabilisierung warten

    TickType_t next_wake = xTaskGetTickCount();
    for (;;) {
        if (modeGet() == OpMode::WORK) {
            uint32_t now_ms = hal_millis();

            // === Modul-Pipeline (alle aktiven Module) ===
            moduleSysRunInput(now_ms);
            moduleSysRunProcess(now_ms);
            moduleSysRunOutput(now_ms);
        }
        // CONFIG: Pipeline angehalten, nur WDT-Feed über task_slow

        vTaskDelayUntil(&next_wake, pdMS_TO_TICKS(TASK_FAST_INTERVAL_MS));
    }
}
```

**Pipeline-Module in WORK-Modus** (Reihenfolge aus `boot_order`):
1. **IMU** → `input()`: SPI-Transfer BNO085, Heading/Roll/Yaw lesen
2. **WAS** → `input()`: SPI-Transfer ADS1118, Lenkwinkel lesen
3. **ACTUATOR** → `input()`: Aktueller PWM-State lesen
4. **SAFETY** → `input()`: Safety-Pin + Watchdog-State lesen
5. **GNSS** → `input()`: UART-Puffer UM980 auslesen
6. **NETWORK** → `input()`: UDP-Empfang AgOpenGPS PGNs parsen
7. **NTRIP** → `input()`: RTCM-Ringbuffer lesen (vom NTRIP-Sub-Task gefüllt)
8. **STEER** → `process()`: PID-Regelung berechnen
9. **ACTUATOR** → `output()`: PWM an DRV8263 senden
10. **NETWORK** → `output()`: PGN 253 Status an AgOpenGPS senden
11. **LOGGING** → `output()`: Telemetrie in SD-Ringbuffer schreiben

**Invarianten:**
- Kein `hal_log()` / `DBG.print()` im Hotpath (nur über geplante Diag-Punkte)
- Shared-State-Zugriff **immer** unter `StateLock` (ADR-STATE-001)
- `SharedSlot<T>` dirty/stale Prüfung bei jeder Eingabe
- Bei stale Daten → `ModuleResult { false, ERR_STALE }` → Modul überspringt Verarbeitung

### 2.2 task_slow — Hintergrunddienste

**Zweck:** Alle nicht-zeitkritischen Operationen. Darf blockieren.

| Eigenschaft | Wert |
|-------------|------|
| Kern | Core 0 |
| Priorität | `configMAX_PRIORITIES - 4` (mittel) |
| Frequenz | Variabel (~100 Hz Poll, 1 Hz HW-Status) |
| Stack | 8192 Bytes (größer für CLI/Config-Menü) |
| Blocking | Erlaubt (TCP connect, SD write, Serial-Read) |

**Verantwortlichkeiten in WORK-Modus:**
1. `DBG.loop()` — TCP Accept/Input/Disconnect (non-blocking Poll)
2. Serial CLI Polling — Zeilenbasiertes Einlesen + `cliProcessLine()`
3. HW-Status-Monitoring (~1 Hz) — `hwStatusUpdate()` mit `g_nav`-Auswertung
4. Telemetrie-Logging (5s Intervall) — Status-Übersicht wenn CLI ruhig
5. Watchdog-Feed — `esp_task_wdt_reset()`
6. Sub-Task Lifecycle — NTRIP-Worker, SD-Flush-Worker überwachen

**Zusätzliche Verantwortlichkeiten in CONFIG-Modus:**
7. Setup-Wizard — `setupWizardRun()` bei Pending
8. Volle CLI — Alle Kommandos erlaubt (inkl. `module activate/deactivate`)
9. Config-Menü — `configMenuRun()` für interaktive Konfiguration
10. Modul-Diagnose — `module debug XY` mit vollem Output

**Ablauf (vereinfacht):**

```cpp
void taskSlowFunc(void*) {
    vTaskDelay(pdMS_TO_TICKS(2000));  // Stabilisierung warten

    for (;;) {
        // 1. TCP-Console (non-blocking)
        DBG.loop();

        // 2. Setup Wizard (nur CONFIG)
        if (modeGet() == OpMode::CONFIG && setupWizardConsumePending()) {
            setupWizardRun();
        }

        // 3. Watchdog
        esp_task_wdt_reset();

        // 4. HW-Status (~1 Hz)
        uint32_t now = hal_millis();
        if (now - last_hw_status_ms >= 1000) {
            hwStatusUpdate(...);
        }

        // 5. Telemetrie (5s, nur wenn CLI ruhig)
        if (now - last_telemetry_ms >= 5000 && now - last_cli_rx_ms >= 2000) {
            logTelemetry();
        }

        // 6. Serial CLI Polling
        while (Serial.available()) { ... cliProcessLine(line); }

        vTaskDelay(pdMS_TO_TICKS(10));  // 100 Hz Poll-Rate
    }
}
```

### 2.3 Arduino loop()

Nachdem `setup()` beide Tasks gestartet hat, ist `loop()` **leer**:

```cpp
void loop() {
    vTaskDelay(portMAX_DELAY);  // Dauerhaft schlafen
}
```

Begründung: `loop()` läuft auf Core 0 mit IDLE-Priorität, hat keinen eigenen Stack, und wird vom RTOS-Scheduler nur ausgeführt wenn kein anderer Task auf Core 0 bereit ist. Wenn ein Sub-Task blocking I/O macht, verhungert `loop()`. Daher: Alle Logik in `task_slow`.

### 2.4 Sub-Tasks

Module mit lang laufenden oder blockierenden I/O-Operationen spawnen **einen** dedizierten Sub-Task auf Core 0. Sub-Tasks werden von `task_slow` gestartet und beendet — `task_slow` ist der **Lifecycle-Owner**.

**Regeln:**
- Maximal ein Sub-Task pro Modul
- Sub-Tasks an Core 0 gepinnt
- Kommunikation mit `task_fast` **ausschließlich** über `SharedSlot<T>` + `StateLock`
- Sub-Tasks dürfen `task_fast` niemals direkt aufrufen oder blockieren
- Sub-Tasks melden sich beim Modulsystem an (Task-Handle + Status)

**Bestehende Sub-Tasks:**

| Sub-Task | Gestartet von | Blocking-Operation | Shared State |
|----------|--------------|-------------------|--------------|
| `maintTask` | `sdLoggerMaintInit()` oder `main.cpp` | NTRIP TCP connect (5s Timeout), SD flush | `g_ntrip` (RTCM-Daten), SD-Ringbuffer |

**Sub-Task: NTRIP-Worker (zukünftig aufgeräumt)**

```
task_slow启动 maintTask
    │
    ├── NTRIP TCP connect (blocking, 5s timeout)
    │       └── Bei Erfolg: RTCM-Daten in Ringbuffer schreiben
    │           └── Ringbuffer → SharedSlot<NtripData> (dirty=true)
    │               └── task_fast input(NTRIP) liest SharedSlot
    │                   └── stale? → skip. dirty? → verarbeiten.
    │
    └── SD flush (periodisch, blocking)
            └── SD-Ringbuffer → physische SD-Karte
```

### 2.5 Datenfluss: Shared State

Alle Cross-Task-Daten fließen über **zwei Mechanismen**:

#### A) NavigationState (bestehend, ADR-STATE-001)

Die bestehende `g_nav`-Struktur bleibt der primäre Shared-State für alle Modul-Daten. Ownership-Regeln aus `state_structs.h` gelten weiterhin:

| Sub-Struct | Writer | Reader |
|-----------|--------|--------|
| `ImuState` | `mod_imu.cpp` | `mod_steer.cpp`, `mod_network.cpp`, `mod_logging.cpp` |
| `SteerState` | `mod_steer.cpp` | `mod_network.cpp`, `mod_logging.cpp` |
| `SwitchState` | `mod_network.cpp` | `mod_steer.cpp`, `mod_logging.cpp` |
| `PidConfigState` | `mod_steer.cpp`, `mod_network.cpp` | `mod_network.cpp` |
| `SafetyState` | `mod_steer.cpp` | `mod_network.cpp`, `mod_logging.cpp` |
| `GnssState` | `mod_network.cpp` | `mod_network.cpp` |

**Zugriff immer unter `StateLock`:**
```cpp
// Writer (task_fast oder task_slow):
{
    StateLock lock;
    g_nav.imu.heading_deg = new_heading;
    g_nav.imu.heading_timestamp_ms = hal_millis();
    g_nav.imu.heading_quality_ok = quality_ok;
}

// Reader (task_fast):
{
    StateLock lock;
    float heading = g_nav.imu.heading_deg;
    uint32_t ts = g_nav.imu.heading_timestamp_ms;
    bool ok = g_nav.imu.heading_quality_ok;
}
if (now_ms - ts < FRESHNESS_MS && ok) {
    // Daten verwenden
} else {
    // Daten veraltet → Fallback
}
```

#### B) SharedSlot<T> (neu, für Sub-Task → task_fast)

Für Daten die von Sub-Tasks (NTRIP-Worker, SD-Logger) an task_fast fließen:

```cpp
template<typename T>
struct SharedSlot {
    T        data {};               // Nutzdaten
    uint32_t last_update_ms = 0;    // Timestamp letzter Schreibzugriff
    bool     dirty = false;         // true = neue Daten seit letztem Lesen
    uint8_t  source_id = 0;         // Writer-Identifikation (ModuleId)
};
```

**Einsatzgebiete:**

| SharedSlot | Writer (Sub-Task) | Reader (task_fast) | Freshness |
|-----------|-------------------|--------------------|-----------| 
| `SharedSlot<RtcmChunk>` | NTRIP-Worker | `mod_ntrip.input()` | 500 ms |
| `SharedSlot<SdStatus>` | SD-Flush-Worker | `mod_logging.input()` | 5000 ms |

**Konsum-Pattern in task_fast:**
```cpp
// mod_ntrip.cpp input():
StateLock lock;
if (s_rtcm_slot.dirty && (now_ms - s_rtcm_slot.last_update_ms < 500)) {
    RtcmChunk chunk = s_rtcm_slot.data;  // Kopie unter Lock
    s_rtcm_slot.dirty = false;
    // RTCM-Daten an GNSS-Receiver weiterleiten
    hal_gnss_write_rtcm(UART_INST, chunk.data, chunk.len);
    return MOD_OK;
}
return { false, ERR_STALE };
```

**Produzent-Pattern in Sub-Task:**
```cpp
// NTRIP-Worker (maintTask):
int bytes = hal_tcp_read(s_ntrip_socket, buf, sizeof(buf));
if (bytes > 0) {
    StateLock lock;
    memcpy(s_rtcm_slot.data.data, buf, bytes);
    s_rtcm_slot.data.len = bytes;
    s_rtcm_slot.last_update_ms = hal_millis();
    s_rtcm_slot.dirty = true;
    s_rtcm_slot.source_id = static_cast<uint8_t>(ModuleId::NTRIP);
}
```

---

## 3. Betriebsmodi: CONFIG ↔ WORK

### 3.1 Zustandsmaschine

Zwei Modi, ersetzen BOOTING/ACTIVE/PAUSED aus ADR-005:

```
                  Safety HIGH
    ┌─────────┐ ──────────────────→ ┌─────────┐
    │  CONFIG │                     │   WORK   │
    │         │ ←────────────────── │         │
    └─────────┘  Safety LOW +       └─────────┘
                  speed < 0.1 km/h
                  oder CLI "mode config"
```

| Modus | task_fast | task_slow |
|-------|-----------|-----------|
| **CONFIG** | Angehalten (nur WDT-Feed über task_slow) | Voll aktiv: CLI, Setup-Wizard, Diagnose, Module aktivieren/deaktivieren, Config-Menü |
| **WORK** | Pipeline @ TASK_FAST_HZ | Reduziert: HW-Monitoring, DBG.loop(), CLI (eingeschränkt), WDT, Telemetrie |

### 3.2 Übergangsbedingungen

| Übergang | Bedingung | Implementierung |
|----------|-----------|-----------------|
| → CONFIG (Boot) | Safety-Pin LOW nach `setup()` | `bootEnterMode()` bleibt in CONFIG |
| → WORK (Boot) | Safety-Pin HIGH nach `setup()` UND Pipeline bereit | `bootEnterMode()` → `modeSet(WORK)` |
| WORK → CONFIG | Safety-Pin LOW **UND** speed < 0.1 km/h **ODER** CLI `mode config` | `task_slow` prüft bei jedem HW-Status-Zyklus |
| CONFIG → WORK | Safety-Pin HIGH **ODER** CLI `mode work` UND Pipeline-Check | `modeSet(WORK)` prüft IMU+WAS+ACT+SAFETY+STEER |

### 3.3 Mode-Management im Modulsystem

Das Modulsystem (`module_interface.h`) definiert bereits:
```cpp
enum class OpMode : uint8_t {
    CONFIG = 0,  // ≡ SETUP
    WORK   = 1   // ≡ RUN
};
```

API:
```cpp
OpMode modeGet(void);                          // Atomic read
bool   modeSet(OpMode target);                 // CONFIG→WORK prüft Pipeline
const char* modeToString(OpMode mode);         // "CONFIG" / "WORK"
```

**CONFIG → WORK Prüfung** (in `modeSet()`):
```cpp
bool steer_ok = moduleSysIsActive(IMU) &&
                moduleSysIsActive(WAS) &&
                moduleSysIsActive(ACTUATOR) &&
                moduleSysIsActive(SAFETY) &&
                moduleSysIsActive(STEER);
if (!steer_ok) return false;  // Übergang verweigert
```

**WORK → CONFIG** (immer erlaubt — Safety first):
```cpp
// Immer erlaubt — kein Sanity-Check nötig
// task_fast erkennt CONFIG und stoppt Pipeline automatisch
```

### 3.4 Automatischer Moduswechsel (Safety-Trigger)

In `task_slow` wird bei jedem HW-Status-Zyklus (~1 Hz) geprüft:

```cpp
// task_slow, HW-Status-Zyklus:
if (modeGet() == OpMode::WORK) {
    StateLock lock;
    bool safety_ok = g_nav.safety.safety_ok;
    float speed = g_nav.sw.gps_speed_kmh;
    // Lock freigeben vor modeSet (modeSet hat eigenen Lock)
    // Nacheinander lesen ist OK — worst case: eine Zyklus-Verzögerung

    if (!safety_ok && speed < 0.1f) {
        modeSet(OpMode::CONFIG);  // Immer erlaubt
    }
}
```

### 3.5 CLI-Steuerung

| Kommando | Aktion |
|----------|--------|
| `mode config` | `modeSet(OpMode::CONFIG)` |
| `mode work` | `modeSet(OpMode::WORK)` (prüft Pipeline) |
| `mode` | Zeigt aktuellen Modus |

---

## 4. Frequenz-Konfiguration

### 4.1 PlatformIO Build-Flags

```ini
[env:lilygo_t_eth_lite_s3]
build_flags =
    -DTASK_FAST_HZ=100       ; Default: 100 Hz
    -DTASK_FAST_STACK=4096   ; Stack-Größe (Bytes)

[env:lilygo_t_eth_lite_s3_200hz]
build_flags =
    -DTASK_FAST_HZ=200       ; High-Performance Profil
    -DTASK_FAST_STACK=4096
```

### 4.2 Runtime-Berechnung

```cpp
#ifndef TASK_FAST_HZ
#define TASK_FAST_HZ 100
#endif
static constexpr uint32_t TASK_FAST_INTERVAL_MS = 1000 / TASK_FAST_HZ;
```

### 4.3 Frequenz-Begründung

| Rate | Anwendungsfall | Begründung |
|------|---------------|------------|
| 50 Hz | Minimal | PID braucht mindestens 50 Hz. AgOpenGPS sendet bei ~10 Hz. |
| **100 Hz** | **Default** | **2× PID-Rate = Sicherheitspuffer. Jitter < 1 ms bei 10 ms Zyklus.** |
| 200 Hz | Performance | Aktueller controlTask-Wert. Nur wenn messbar nötig. |

**100 Hz ist ausreichend weil:**
- AgOpenGPS sendet PGNs bei ~10 Hz → 10 Zyklen Buffer
- PID-Regelung braucht ~50 Hz → 2× Oversampling
- ESP32-S3 @ 240 MHz: Pipeline-Zyklus < 1 ms bei 100 Hz → 90 % CPU-Reserve
- StateLock-Overhead: ~5 µs × 5 Reads = 25 µs → 0,25 % bei 10 ms Zyklus

---

## 5. Migration von ADR-002

### 5.1 Mapping

| ADR-002 (alt) | ADR-007 (neu) | Migration |
|---------------|---------------|-----------|
| `controlTask` (Core 1, 200 Hz) | `task_fast` (Core 1, konfigurierbar) | Funktion direkt übernommen, Frequenz konfigurierbar |
| `commTask` (Core 0, 100 Hz) | **entfällt** | HW-Status-Monitoring → `task_slow` |
| `maintTask` (Core 0, variabel) | **Sub-Task unter task_slow** | Lifecycle-Owner ändert sich |
| `loop()` (Core 0, ~10 Hz) | **leer** | DBG.loop() + CLI → `task_slow` |

### 5.2 Inkonsistenz: Zwei OpMode-Systeme

**Problem:** Es existieren zwei konkurrierende OpMode-Definitionen:

1. **`op_mode.h`** (C-Style, ADR-005):
   ```c
   typedef enum { OP_MODE_BOOTING, OP_MODE_ACTIVE, OP_MODE_PAUSED } OpMode;
   ```
   Implementiert in `op_mode.cpp` mit: Atomic, Mutex, GPIO-Toggle, NVS-Persistenz, AgIO-Notification.

2. **`module_interface.h`** (C++-Style, ADR-MODULE-002):
   ```cpp
   enum class OpMode : uint8_t { CONFIG = 0, WORK = 1 };
   ```
   Implementiert in `module_system.cpp` mit: Einfacher Vergleich, Pipeline-Check.

**Aktueller Zustand in main.cpp:**
```cpp
// main.cpp includes module_interface.h (OpMode::CONFIG/WORK)
// main.cpp does NOT include op_mode.h (explicitly avoided)
// bootEnterMode() uses modeSet(OpMode::WORK) from module_system
```

**Entscheidung:**
- `module_interface.h` OpMode (CONFIG/WORK) ist **die authoritative Quelle**
- `op_mode.h/cpp` wird **obsolet** und soll bereinigt werden
- Nützliche Features aus `op_mode.cpp` migrieren:
  - ~~GPIO-Toggle~~ → wird Follow-Up (erst wenn CONFIG→WORK Transition stabil)
  - ~~NVS-Persistenz~~ → Nice-to-have, nicht kritisch
  - ~~AgIO-Notification~~ → PGN 253 `paused`-Bit wird in `mod_network.output()` gesetzt

### 5.3 Bereinigungsplan für op_mode.h/cpp

| Feature | Aktion | Priority |
|---------|--------|----------|
| `op_mode.h` Enum-Definition | **Löschen** — durch `module_interface.h` ersetzt | P0 |
| `op_mode.cpp` State Machine | **Löschen** — durch `module_system.cpp` ersetzt | P0 |
| `op_mode.cpp` GPIO-Toggle | **Migrieren** nach `task_slow` (Follow-Up Task) | P2 |
| `op_mode.cpp` NVS-Persistenz | **Migrieren** nach `module_system.cpp` (Follow-Up) | P2 |
| `op_mode.cpp` AgIO paused-Bit | **Migrieren** nach `mod_network.cpp` | P1 |
| Alle `#include "op_mode.h"` | **Ersetzen** durch `#include "module_interface.h"` | P0 |
| Alle `opModeGet()` Aufrufe | **Ersetzen** durch `modeGet()` | P0 |
| Alle `opModeRequest()` Aufrufe | **Ersetzen** durch `modeSet()` | P0 |
| `g_nav.sw.paused` Flag | **Beibehalten** — wird von `modeSet()` gesetzt | P1 |

---

## 6. Sub-Task Strategie (Empfehlung)

### 6.1 Entscheidung: Callbacks vs. dedizierte Tasks

Für die Frage **"Wann ein Sub-Task, wann ein Callback?"** gilt:

| Kriterium | Callback (in task_slow) | Dedizierter Sub-Task |
|-----------|----------------------|---------------------|
| Dauer | < 50 ms | > 50 ms oder unbegrenzt |
| Blocking | Nein | Ja (TCP connect, SD write) |
| Frequenz | Seltene One-Shot-Operation | Periodisch oder kontinuierlich |
| Beispiel | Module activate/deactivate | NTRIP TCP-Verbindung, SD-Flush |
| Stack-Bedarf | Teilt task_slow Stack (8 KB) | Eigener Stack (4 KB) |

### 6.2 Aktuelle Sub-Tasks

#### maintTask (NTRIP + SD)

Der bestehende `maintTask` deckt zwei Verantwortlichkeiten ab:

1. **NTRIP TCP-Connect/Reconnect** — Blocking (5s Timeout), kontinuierlich
2. **SD-Flush** — Periodisch, Blocking

**Empfehlung:** Beide bleiben im `maintTask` (Sub-Task). Eine Aufteilung ist nicht nötig — der `maintTask` hat bereits eine saubere State-Machine in `sdLoggerMaintInit()` / `maintTaskFunc()`.

**Lifecycle:**
```
Boot:  main.cpp oder mod_logging.cpp → sdLoggerMaintInit()
                                            │
Run:   maintTask läuft auf Core 0           │
       ├── ntripTick()  (NTRIP TCP)         │
       └── sdFlushTick() (SD Card)          │
                                            │
Stop:  (derzeit kein sauberer Stop-Mechanismus → Follow-Up)
```

### 6.3 Zukünftige Sub-Tasks (nicht in Scope)

| Sub-Task | Auslöser | Grund |
|----------|----------|-------|
| WiFi-AP Manager | CONFIG-Modus aktiv | blocking DHCP, Web-OTA |
| BT SPP Manager | CONFIG-Modus aktiv | blocking Serial |
| SD-Export Worker | CLI `log export` | Large file copy |

---

## 7. Watchdog-Strategie

### 7.1 TWDT (Task Watchdog Timer)

Beide Tasks müssen den TWDT füttern:

| Task | TWDT-Feed | Interval |
|------|-----------|----------|
| `task_fast` | `esp_task_wdt_reset()` | Implizit (kein eigener Feed nötig wenn Task regelmäßig läuft) |
| `task_slow` | `esp_task_wdt_reset()` | Jeder Zyklus (10 ms Poll) |

**task_fast** füttert den TWDT **nicht explizit** — stattdessen wird der TWDT so konfiguriert, dass er nur Core 0 überwacht (task_slow ist verantwortlich). Wenn task_fast hängt, wird er vom Priority-Watchdog erkannt (nicht vom TWDT).

**Alternative (bevorzugt):** Beide Tasks subscriben am TWDT. task_fast füttert implizit (regelmäßiger `vTaskDelayUntil`). task_slow füttert explizit `esp_task_wdt_reset()`.

### 7.2 Hardware-Watchdog (External)

Der externe Hardware-Watchdog (DRV8263 WDI) wird von `mod_safety.cpp` verwaltet und ist **unabhängig von der Task-Architektur**. Er bleibt unverändert.

---

## 8. Fehlerbehandlung

### 8.1 task_fast Fehler

| Fehler | Erkennung | Reaktion |
|--------|-----------|----------|
| Modul input() returns `!success` | `ModuleResult` Check | Modul überspringt process/output |
| Stale-Daten (freshness timeout) | `now_ms - last_update_ms > timeout` | Modul meldet unhealthy |
| Pipeline-Jitter > Schwellwert | Zykluszeit-Messung | DBG.log() Warnung |
| task_fast hängt (Priority-WDT) | FreeRTOS Priority-WDT | System-Reset |

### 8.2 task_slow Fehler

| Fehler | Erkennung | Reaktion |
|--------|-----------|----------|
| TWDT-Reset | task_slow füttert nicht mehr | System-Reset |
| Sub-Task Absturz | Task-Handle Check | task_slow loggt + recreates (optional) |
| SD-Karte entfernt | `hwStatusUpdate()` | mod_logging deaktiviert |
| ETH-Link-Down | `hwStatusUpdate()` | PGN 253 Status-Bit setzen |

### 8.3 Sub-Task Fehler

| Fehler | Erkennung | Reaktion |
|--------|-----------|----------|
| NTRIP TCP-Connect Timeout | `ntripTick()` returns error | State Machine → IDLE → Reconnect |
| NTRIP Auth-Failure | HTTP 401 | State Machine → ERROR → Delayed Reconnect |
| SD-Write-Fehler | `sdFlushTick()` returns error | Log-Error, Modul unhealthy melden |

---

## 9. Datei-Layout

### 9.1 Neue Dateien

```
src/logic/shared_state.h          ← BEREITS VORHANDEN (SharedSlot<T>)
docs/adr/ADR-007-two-task-architecture.md  ← BEREITS VORHANDEN
```

### 9.2 Geänderte Dateien

```
src/main.cpp
  ├── setup(): Boot-Phasen (bereits umgestellt)
  ├── bootStartTasks(): task_fast + task_slow erstellen (bereits umgestellt)
  ├── taskFastFunc(): Pipeline-Funktion (bereits umgestellt)
  ├── taskSlowFunc(): Hintergrund-Funktion (bereits umgestellt)
  └── loop(): vTaskDelay(portMAX_DELAY) (bereits umgestellt)

platformio.ini
  └── build_flags: -DTASK_FAST_HZ=100, -DTASK_FAST_STACK=4096 (bereits vorhanden)
```

### 9.3 Zu bereinigende Dateien (Migration)

```
src/logic/op_mode.h              ← LÖSCHEN (obsolet, ersetzt durch module_interface.h)
src/logic/op_mode.cpp            ← LÖSCHEN (obsolet, ersetzt durch module_system.cpp)
  ├── Features migrieren:
  │   ├── GPIO-Toggle → task_slow (Follow-Up)
  │   ├── NVS-Persistenz → module_system.cpp (Follow-Up)
  │   └── AgIO paused-Bit → mod_network.cpp (P1)
  └── Alle Referenzen aktualisieren:
      ├── opModeGet() → modeGet()
      ├── opModeRequest() → modeSet()
      ├── opModeIsPaused() → modeGet() == OpMode::CONFIG
      └── OP_MODE_ACTIVE → OpMode::WORK, OP_MODE_PAUSED → OpMode::CONFIG
```

### 9.4 Potenziell geänderte Module-Dateien

```
src/logic/mod_network.cpp   ← op_mode Referenzen entfernen, paused-Bit setzen
src/logic/mod_ntrip.cpp     ← Sub-Task Lifecycle an task_slow anpassen
src/logic/mod_logging.cpp   ← maintTask-Start sauberer gestalten
src/logic/mod_steer.cpp     ← op_mode Referenzen entfernen
```

---

## 10. Implementierungsplan

### Phase 1: Bereinigung op_mode.h/cpp (P0)

1. Alle `#include "op_mode.h"` Referenzen finden und durch `module_interface.h` ersetzen
2. Alle `opModeGet()` → `modeGet()`, `opModeRequest()` → `modeSet()` etc.
3. `op_mode.h` und `op_mode.cpp` löschen
4. `g_nav.sw.paused` Flag in `modeSet()` setzen (bei CONFIG → `paused=true`, bei WORK → `paused=false`)
5. Build prüfen, alle Compiler-Errors beheben

### Phase 2: Sub-Task Lifecycle (P1)

1. `maintTask` Start sauberer gestalten:
   - Einheitlicher Startpunkt (nur über `task_slow` oder `mod_logging.activate()`)
   - Kein direkter Aufruf aus `main.cpp::bootStartTasks()`
2. Sub-Task-Registrierung im Modulsystem (optional: Task-Handle + Status)
3. `mod_network.cpp`: `paused`-Bit in PGN 253 Status setzen

### Phase 3: SharedSlot Integration (P1)

1. NTRIP RTCM-Datenfluss über `SharedSlot<RtcmChunk>`
2. task_fast `mod_ntrip.input()` liest aus SharedSlot statt globalem Puffer
3. NTRIP-Sub-Task schreibt in SharedSlot mit dirty-flag

### Phase 4: Follow-Up (P2)

1. GPIO-Toggle Mode-Switch in `task_slow` implementieren
2. NVS-Persistenz für CONFIG/WORK Modus in `module_system.cpp`
3. OpMode-Umbenennung (CONFIG → SETUP, WORK → RUN) — wenn Breaking-Change akzeptiert

---

## 11. Risiken & Mitigation

| Risiko | Wahrscheinlichkeit | Auswirkung | Mitigation |
|--------|-------------------|------------|------------|
| task_slow wird SPOF (Single Point of Failure) | Mittel | Hoch — alle Hintergrunddienste aus | TWDT erzeugt Reset → automatische Recovery |
| 100 Hz statt 200 Hz für Pipeline | Niedrig | Niedrig — AgOpenGPS bei 10 Hz, PID bei 50 Hz | Konfigurierbar, kann auf 200 Hz erhöht werden |
| StateLock Overhead im Hotpath | Niedrig | Niedrig — ~25 µs bei 10 ms Zyklus | Messung bei 100 Hz, nur bei Problemen optimieren |
| op_mode.cpp Referenzen vergessen | Mittel | Mittel — Compile-Error oder Runtime-Fehler | `rg "op_mode" src/` vor Löschen — keine Treffer erlaubt |
| maintTask Lifecycle unsauber | Mittel | Mittel — Task läuft nach Deaktivierung weiter | Sauberer Start/Stop-Mechanismus in Phase 2 |

---

## 12. Messgrößen & Validierung

### 12.1 Task-Monitoring

```
[DBG-FAST] 99.8 Hz  (task_fast Pipeline-Rate, 1s Intervall)
SLOW: HW error count -> 0  (task_slow HW-Status, bei Änderung)
```

### 12.2 Akzeptanzkriterien (aus TASK-047)

- [ ] `setup()` erstellt genau zwei FreeRTOS-Tasks (task_fast, task_slow)
- [ ] task_fast läuft auf Core 1 mit konfigurierbarer Frequenz (default 100 Hz)
- [ ] task_slow läuft auf Core 0 mit variabler Frequenz
- [ ] Arduino `loop()` enthält keine eigene Logik mehr
- [ ] task_fast ruft `moduleSysRunInput/Process/Output()` im WORK-Modus auf
- [ ] task_fast ist angehalten im CONFIG-Modus (Pipeline läuft nicht)
- [ ] Pipeline-Frequenz ist stabil (Jitter < 1 ms)
- [ ] HW-Status-Monitoring (~1 Hz) funktioniert
- [ ] DBG.loop() wird regelmäßig aufgerufen
- [ ] Serial/TCP CLI-Polling funktioniert
- [ ] WDT wird gefüttert
- [ ] `SharedSlot<T>` Template implementiert
- [ ] CONFIG-Modus: task_slow voll aktiv, task_fast angehalten
- [ ] WORK-Modus: task_fast Pipeline, task_slow reduziert
- [ ] Übergang CONFIG → WORK und WORK → CONFIG funktioniert
- [ ] maintTask läuft als Sub-Task unter task_slow
- [ ] `TASK_FAST_HZ` per PlatformIO-Profil setzbar
- [ ] Build mit TASK_FAST_HZ=100 und TASK_FAST_HZ=200 erfolgreich

---

## 13. Offene Fragen

1. **Soll `g_nav.sw.paused` in `modeSet()` gesetzt werden?**
   → Empfehlung: Ja. `modeSet(CONFIG)` → `g_nav.sw.paused = true`, `modeSet(WORK)` → `false`. Das PGN 253 Status-Byte wird dann in `mod_network.output()` automatisch korrekt gesendet.

2. **Soll der `maintTask` zwingend über `task_slow` gestartet werden?**
   → Empfehlung: Ja, aber pragmatisch. Aktuell wird er von `mod_logging.activate()` oder `main.cpp` gestartet. Die Bereinigung kann schrittweise erfolgen. Wichtig: `task_slow` MUSS wissen dass der `maintTask` läuft (für Fehlerbehandlung).

3. **Soll die GPIO-Toggle-Funktionalität aus `op_mode.cpp` übernommen werden?**
   → Empfehlung: Nicht in TASK-047. Follow-Up mit simplerer Implementierung in `task_slow` (1 Hz GPIO-Poll, Edge-Detection, `modeSet()` Aufruf).

4. **Soll NVS-Persistenz für den Modus beibehalten werden?**
   → Empfehlung: Nice-to-have, Follow-Up. Safety-Pin hat Vorrang — NVS-Persistenz ist nur Komfort für den Fall dass der Safety-Pin beim Boot den falschen Zustand hat.

5. **SharedSlot für NTRIP RTCM-Daten — sofort oder schrittweise?**
   → Empfehlung: Schrittweise. Der globale NTRIP-Puffer funktioniert bereits. SharedSlot-Integration kann erfolgen wenn der aktuelle Datenfluss стабилизiert ist.

---

## 14. Zusammenfassung

| Aspekt | Vorher (ADR-002) | Nachher (ADR-007) |
|--------|-------------------|-------------------|
| Tasks | 3 + loop() = 4 Kontexte | 2 (+ Sub-Tasks) |
| Kerne | Core 0: comm + maint + loop, Core 1: control | Core 0: slow (+ subs), Core 1: fast (exklusiv) |
| Modi | BOOTING / ACTIVE / PAUSED (3 Zustände) | CONFIG / WORK (2 Zustände) |
| Frequenz | controlTask fix 200 Hz | task_fast konfigurierbar, default 100 Hz |
| Mode-System | `op_mode.h` (C, Mutex, Atomic, GPIO, NVS) | `module_interface.h` (C++, einfach) |
| Shared State | `g_nav` + `StateLock` | `g_nav` + `StateLock` + `SharedSlot<T>` |
| Sub-Tasks | maintTask (inkonsistenter Start) | maintTask (sauberer Lifecycle unter task_slow) |
| loop() | DBG.loop(), CLI, WDT, Boot-Maint | Leer (`vTaskDelay(portMAX_DELAY)`) |

**Kernbotschaft:** Die Architektur ist **bereits großteils implementiert** (ADR-007 akzeptiert, main.cpp umgestellt, SharedSlot vorhanden). Der verbleibende Aufwand besteht in der **Bereinigung des alten op_mode-Systems** und der **sauberen Sub-Task-Integration**. CODE-ÄNDERUNGEN sollten **nach Freigabe dieses Gesamtkonzepts** beginnen.
