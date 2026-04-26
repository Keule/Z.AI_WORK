# ZAI_GPS — Modul-Architektur Refactoring Brief

> **⚠️ HISTORICAL DOCUMENT — Pre-Refactoring State (April 2026)**
>
> This brief was written before the refactoring phases (1–4) were executed.
> All seven identified problems (P1–P7) have been resolved:
>
> | Problem | Resolution |
> |---------|-----------|
> | P1 HAL Init feature-blind | Feature gates in `hal_esp32_init_all()` |
> | P2 Logic-Init orphaned | Removed dead `imuInit()`, `steerAngleInit()`, `actuatorInit()` |
> | P3 Kein einheitliches Modul-Interface | ModuleOps2 unified module system (ADR-MODULE-002) |
> | P4 Doppelte State-Schreibungen | WAS writes to SteerState only, PID output separate |
> | P5 Monolithischer Global State | Sub-structs in `state_structs.h` with ownership rules |
> | P6 controlStep() buendelt alles | Replaced by module pipeline: `mod_safety.input()` → `mod_steer.process()` → `mod_steer.output()` |
> | P7 RTCM ohne Feature-Gate | `FEAT_GNSS` gate applied |
>
> **Current architecture**: ADR-007 Two-Task model (task_fast + task_slow) with ModuleOps2 pipeline.
> See `docs/GESAMTKONZEPT-two-task-architecture.md` and `docs/adr/ADR-007-two-task-architecture.md`.
>
> Files referenced below (`control.cpp`, `modules.cpp`, `steer_angle.cpp`, `actuator.cpp`) **no longer exist**.
> They were replaced by `mod_steer.cpp`, `mod_imu.cpp`, `mod_was.cpp`, `mod_safety.cpp`, `mod_actuator.cpp`, etc.

---

> **Zweck:** Dieser Brief richtet sich an KI-Agenten, die Code-Aenderungen an der
> ZAI_GPS Firmware vornehmen sollen. Er enthaelt die vollstaendige Architektur-Analyse,
> alle identifizierten Probleme, die Refactoring-Vorschlaege und den noetigen Kontext,
> der in vorherigen Konversationen implizit vorausgesetzt wurde.
>
> **Erstellt:** 2026-04-23 | **Analyst:** Z.ai Super-Agent
> **Basis:** `Keule/ESP32_AGO_GNSS`, Branch `development`, Commit-Stand April 2026

---

## 1. Repository-Kontext und Zugangsdaten

| Attribut | Wert |
|----------|------|
| **Repository** | `https://github.com/Keule/ESP32_AGO_GNSS` |
| **Aelterer Name** | `Keule/ZAI_GPS` (GitHub leitet automatisch um) |
| **Standard-Branch** | `development` (NICHT `main`) |
| **Plattform** | PlatformIO (ESP32-S3) |
| **Target-Hardware** | LilyGO T-ETH-Lite-S3 (ESP32-S3-WROOM-1 + W5500 Ethernet) |
| **RTOS** | FreeRTOS (dual-core) |

### Wichtige Datei-URLs (raw, Branch `development`)

```
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/features.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/global_state.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/global_state.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/control.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/control.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/modules.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/modules.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/imu.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/imu.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/steer_angle.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/steer_angle.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/actuator.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/actuator.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/net.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/net.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/hw_status.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/hw_status.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/sd_logger.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/sd_logger.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/ntrip.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/ntrip.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/pgn_codec.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/pgn_codec.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/dependency_policy.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/logic/dependency_policy.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/hal/hal.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/hal_esp32/hal_impl.h
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/hal_esp32/hal_impl.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/hal_esp32/hal_bno085.cpp
https://raw.githubusercontent.com/Keule/ESP32_AGO_GNSS/development/src/main.cpp
```

### Verzeichnisstruktur

```
src/
├── hal/                      # HAL-Interface (pure C declarations)
│   └── hal.h                 # Zentrale HAL-Schnittstelle
├── hal_esp32/                # ESP32-spezifische HAL-Implementierung
│   ├── hal_impl.h
│   ├── hal_impl.cpp          # ~71 KB, enthaelt hal_esp32_init_all()
│   ├── hal_bno085.cpp        # BNO085 IMU SPI-Treiber
│   ├── sd_logger_esp32.cpp   # SD-Logger Plattform-Code
│   └── sd_ota_esp32.cpp      # OTA-Update Plattform-Code
├── logic/                    # Plattformunabhaengige Logik-Schicht
│   ├── features.h            # Feature-Flag-System (~10.7 KB)
│   ├── global_state.h/.cpp   # NavigationState + Mutex
│   ├── control.h/.cpp        # PID + controlStep() (~9.4 KB)
│   ├── imu.h/.cpp            # IMU-Logik (~9.2 KB)
│   ├── steer_angle.h/.cpp    # WAS-Logik (~849 B)
│   ├── actuator.h/.cpp       # Aktor-Logik (~466 B)
│   ├── net.h/.cpp            # UDP/PGN Kommunikation (~19.8 KB)
│   ├── modules.h/.cpp        # Modul-Registry (~23.4 KB / 8.3 KB)
│   ├── hw_status.h/.cpp      # Hardware-Status-Monitoring
│   ├── sd_logger.h/.cpp      # SD-Logger Logik
│   ├── ntrip.h/.cpp          # NTRIP-Client
│   ├── pgn_codec.h/.cpp      # PGN-Protokoll-Codec
│   ├── pgn_types.h           # PGN-Datentypen
│   ├── pgn_registry.h        # PGN-Registrierung
│   ├── dependency_policy.h/.cpp  # Modul-Abhaengigkeiten
│   ├── runtime_config.h/.cpp    # Laufzeit-Konfiguration
│   └── log_*.h/.cpp          # Logging-Subsystem
└── main.cpp                  # Entry-Point (~35 KB)
```

### Build-Profile (platformio.ini)

Das Build-System definiert vier Profile ueber `-D` Makros:

| Profil | Macro | Beschreibung |
|--------|-------|-------------|
| `COMM_ONLY` | `-DFEAT_PROFILE_COMM_ONLY` | Nur Ethernet/UDP Kommunikation, kein Sensor, kein Aktor |
| `SENSOR_FRONT` | `-DFEAT_PROFILE_SENSOR_FRONT` | Vorne: IMU + WAS-Sensor |
| `ACTOR_REAR` | `-DFEAT_PROFILE_ACTOR_REAR` | Hinten: Aktor |
| `FULL_STEER` | `-DFEAT_PROFILE_FULL_STEER` | Alles: IMU + WAS + Aktor |

Zusaetzliche Build-Modes (exklusiv zu den Profilen):
- `-DFEAT_IMU_BRINGUP` — IMU-Diagnose-Mode (interaktive SPI-Bring-Up)
- `-DFEAT_GNSS_BUILDUP` — GNSS-Integrations-Mode (NTRIP + UART-Mirror)

---

## 2. Architektur-Prinzipien (Warum das Repo so ist)

### 2.1 PLC-Dreischritt-Muster

Die Firmware folgt bewusst einem klassischen PLC-Muster:

```
EINGABE (Input)  ->  VERARBEITUNG (Processing)  ->  AUSGABE (Output)
     |                        |                          |
  Sensor lesen            PID berechnen            Aktor ansteuern
  UDP empfangen           PGN dekodieren           UDP senden
  GPIO pruefen            Safety-Gating            PWM schreiben
```

### 2.2 Dual-Core FreeRTOS-Architektur

```
+-----------------------------------------------------------+
|                    ESP32-S3 Dual Core                      |
+----------------------------+------------------------------+
|  Core 0                    |  Core 1                      |
|  commTask (100 Hz)         |  controlTask (200 Hz)       |
|                            |                              |
|  [EINGABE]                 |  [EINGABE]                   |
|   netPollReceive()         |   hal_safety_ok()            |
|   ntripReadRtcm()          |   imuUpdate()                |
|                            |   steerAngleReadDeg()        |
|  [VERARBEITUNG]            |                              |
|   PGN decode               |  [VERARBEITUNG]              |
|   modulesUpdateStatus()    |   controlStep() -> PID       |
|                            |   Watchdog-Check             |
|  [AUSGABE]                 |                              |
|   netSendAogFrames()       |  [AUSGABE]                   |
|   gnssMirrorPoll()         |   actuatorWriteCommand()     |
|   ntripForwardRtcm()       |   sdLoggerRecord()           |
|                            |                              |
|  maintTask (niedrig Prio)  |                              |
|   SD-Flush, NTRIP connect  |                              |
+----------------------------+------------------------------+
|              Shared: NavigationState g_nav                   |
|              Geschuetzt durch: StateLock (HAL Mutex)        |
+-----------------------------------------------------------+
```

### 2.3 Schichtenmodell

```
+----------------------------------------------+
|  main.cpp (Task-Erstellung, Setup)           |
+----------------------------------------------+
|  logic/ (Plattformunabhaengig)                |
|  - control.cpp, imu.cpp, net.cpp             |
|  - steer_angle.cpp, actuator.cpp             |
|  - modules.cpp, hw_status.cpp                |
|  - pgn_codec.cpp, ntrip.cpp                  |
+----------------------------------------------+
|  hal/hal.h (Pure-C Interface)                |
+----------------------------------------------+
|  hal_esp32/ (ESP32-S3 Implement.)            |
|  - hal_impl.cpp, hal_bno085.cpp              |
+----------------------------------------------+
```

### 2.4 HAL-Design-Philosophie

Die HAL ist als **Pure-C Interface** (`hal.h`) mit separater ESP32-Implementierung (`hal_esp32/`) konzipiert:

- `hal.h` enthaelt KEINE Arduino- oder ESP32-Header
- Alle HW-Zugriffe gehen ueber `hal_xxx()` Funktionen mit C-Linkage
- Die Logik-Schicht (`logic/`) inkludiert nur `hal.h`, nie ESP32-Header
- Eine PC-Simulation (`hal_pc/`) waere theoretisch moeglich

### 2.5 AgOpenGPS PGN-Protokoll

| PGN | Richtung | Funktion |
|-----|----------|----------|
| 252 | Rx (von AgIO) | Steer-Settings (Kp, PWM-Werte, Kalibrierung) |
| 253 | Tx (an AgIO) | Status-Report (Heading, Roll, Yaw-Rate, Lenkwinkel) |
| 250 | Tx (an AgIO) | Steering-Data (PID-Output, WAS-Wert, Switches) |
| 254 | Rx (von AgIO) | Steer-Data + Heartbeat (Sollwinkel, Speed, Switches) |
| 214 | Tx (an AgIO) | GPS-Status (Fix-Quality, Diff-Age) — geplant |
| 200 | Rx (von AgIO) | Hello-Request -> Hello-Reply |
| 202 | Rx (von AgIO) | Scan-Request -> Subnet-Reply |
| 0xDD | Tx (an AgIO) | Hardware-Error-Report |

### 2.6 Feature-Flag-System (features.h)

Das Flag-System hat **drei Schichten**:

```
Schicht 1: Roh-Flags aus Build-System (platformio.ini -D...)
  FEAT_COMM, FEAT_IMU, FEAT_STEER_SENSOR, FEAT_STEER_ACTOR, ...

Schicht 2: Profil-Defaults (wenn Profil aktiv, aber Roh-Flag fehlt)
  FEAT_CFG_PROF_IMU = (SENSOR_FRONT || FULL_STEER)

Schicht 3: Normierte Flags mit Legacy-Default
  FEAT_IMU = (FEAT_CFG_RAW_IMU || (FEAT_CFG_PROF_IMU && !FEAT_CFG_RAW_IMU) || FEAT_CFG_DEFAULT_ON)
```

Der `feat::` Namespace bietet `constexpr` Abfragen:

```cpp
namespace feat {
  inline constexpr bool imu()    { return FEAT_ENABLED(FEAT_COMPILED_IMU); }
  inline constexpr bool ads()    { return FEAT_ENABLED(FEAT_COMPILED_ADS); }
  inline constexpr bool act()    { return FEAT_ENABLED(FEAT_COMPILED_ACT); }
  inline constexpr bool safety() { return FEAT_ENABLED(FEAT_COMPILED_SAFETY); }
  inline constexpr bool control(){ return act() && safety(); }
  // ...
}
```

### 2.7 Hardware-Feature-Modul-System (modules.h/cpp)

Drei Zustaende pro Modul:

```cpp
enum ModState : int8_t {
  MOD_UNAVAILABLE = -1,  // Nicht compiliert oder Pins nicht verfuegbar
  MOD_OFF         =  0,  // Compiliert, aber nicht aktiv
  MOD_ON          =  1   // Compiliert und aktiv
};
```

Module: `MOD_IMU`, `MOD_ADS`, `MOD_ACT`, `MOD_ETH`, `MOD_GNSS`, `MOD_NTRIP`, `MOD_SAFETY`, `MOD_LOGSW`, `MOD_SD`

Aktivierung in `main.cpp::setup()`:
```cpp
moduleActivate(MOD_IMU);     // IMU: keine Abhaengigkeiten
moduleActivate(MOD_ADS);     // ADS: keine Abhaengigkeiten
moduleActivate(MOD_ETH);     // ETH: keine Abhaengigkeiten
moduleActivate(MOD_GNSS);    // GNSS: keine Abhaengigkeiten
moduleActivate(MOD_SAFETY);  // SAFETY: keine Abhaengigkeiten
moduleActivate(MOD_ACT);     // ACT: haengt von IMU + ADS ab
moduleActivate(MOD_SD);      // SD: bedingt (nur bei erkannter Karte)
moduleActivate(MOD_NTRIP);   // NTRIP: haengt von ETH ab
```

---

## 3. Globaler Zustand (NavigationState)

```cpp
// src/logic/global_state.h
struct NavigationState {
    // --- Heading / IMU ---
    float    heading_deg;
    float    roll_deg;
    float    yaw_rate_dps;
    uint32_t heading_timestamp_ms;
    bool     heading_quality_ok;
    uint32_t imu_timestamp_ms;
    bool     imu_quality_ok;

    // --- Steering ---
    float    steer_angle_deg;
    int16_t  steer_angle_raw;
    uint32_t steer_angle_timestamp_ms;
    bool     steer_angle_quality_ok;
    bool     safety_ok;

    // --- AgIO switches ---
    bool     work_switch;
    bool     steer_switch;
    uint8_t  last_status_byte;

    // --- Steering settings (PGN 252) ---
    uint8_t  settings_kp, settings_high_pwm, settings_low_pwm, settings_min_pwm;
    uint8_t  settings_counts;
    int16_t  settings_was_offset;
    uint8_t  settings_ackerman;
    bool     settings_received;

    // --- Steering config (PGN 251) ---
    uint8_t  config_set0, config_max_pulse, config_min_speed;
    bool     config_received;

    // --- Watchdog ---
    uint32_t watchdog_timer_ms;
    bool     watchdog_triggered;

    // --- Speed safety ---
    float    gps_speed_kmh;

    // --- GNSS/UM980 ---
    uint8_t  gps_fix_quality;
    int16_t  gps_diff_age_x100_ms;
    uint8_t  um980_fix_type;
    bool     um980_rtcm_active;
    uint32_t um980_status_timestamp_ms;

    // --- PID output ---
    uint16_t pid_output;

    // --- Timing ---
    uint32_t timestamp_ms;
};

extern NavigationState g_nav;
extern volatile float desiredSteerAngleDeg;

class StateLock {
public:
    StateLock()  { hal_mutex_lock(); }
    ~StateLock() { hal_mutex_unlock(); }
};
```

**Wichtig:** `g_nav` wird von beiden Tasks (Core 0 und Core 1) geschrieben und gelesen.
Alle Zugriffe MUSSSEN durch `StateLock lock;` gesichert werden.

---

## 4. Aktuelle Modul-Landschaft — Analyse

### 4.1 Modul-Lebenszyklus-Matrix

| Modul | Datei | Feature Flag | HAL Init | Logic Init | Update/Step | Feature Gate | State Write |
|-------|-------|-------------|----------|------------|-------------|-------------|-------------|
| COMM | `net.cpp` | `FEAT_COMM` (immer an) | OK | NEVER | `netPollReceive()` / `netSendAogFrames()` | NEIN | `g_nav` (mehrere Felder) |
| IMU | `imu.cpp` | `FEAT_IMU` | IMMER | NEVER | `imuUpdate()` | NEIN | `heading_deg`, `roll_deg`, `yaw_rate_dps` |
| STEER_ANGLE | `steer_angle.cpp` | `FEAT_STEER_SENSOR` | IMMER | NEVER | `steerAngleReadDeg()` | NEIN | DOPPELT (+ `controlStep`) |
| ACTUATOR | `actuator.cpp` | `FEAT_STEER_ACTOR` | IMMER | NEVER | `actuatorWriteCommand()` | NEIN | kein State |
| CONTROL | `control.cpp` | `FEAT_MACHINE_ACTOR` | IMMER | JA | `controlStep()` | JA | `pid_output`, `safety_ok`, `watchdog` |
| GNSS/RTCM | `net.cpp` | `FEAT_GNSS` (default 0) | IMMER | n/a | `netPollRtcmReceive()` | NEIN | `um980_*` (nur Update) |
| HW_STATUS | `hw_status.cpp` | kein Flag | n/a | IMMER | `hwStatusUpdate()` | NEIN | kein `g_nav` |
| MODULES | `modules.cpp` | intern `feat::*` | n/a | IMMER | `modulesUpdateStatus()` | PARTIELL | kein `g_nav` |
| SD_LOGGER | `sd_logger.cpp` | kein Flag | n/a | FEAT: JA | `sdLoggerRecord()` | PARTIELL | kein `g_nav` |
| NTRIP | `ntrip.cpp` | `FEAT_NTRIP` | n/a | n/a | `ntripTick()` / `ntripReadRtcm()` | JA | `g_ntrip` |

**Legende:**
- **OK** = korrekt implementiert
- **IMMER** = unabhaengig von Feature-Flag ausgefuehrt (Problem!)
- **NEVER** = Funktion existiert, wird nie aufgerufen (Totcode!)
- **NEIN** = Feature-Gate fehlt komplett
- **PARTIELL** = Feature-Gate nur teilweise vorhanden
- **JA** = korrekt geguardet
- **DOPPELT** = State wird an zwei Stellen geschrieben (Redundanz-Risiko)

### 4.2 API-Vergleich der Sensor-/Aktormodule

| Aspekt | IMU (`imu.h`) | STEER_ANGLE (`steer_angle.h`) | ACTUATOR (`actuator.h`) |
|--------|---------------|------------------------------|------------------------|
| Init | `imuInit()` | `steerAngleInit()` | `actuatorInit()` |
| Update | `imuUpdate() -> bool` | `steerAngleReadDeg() -> float` | (kein Update) |
| Output | (schreibt `g_nav`) | (schreibt `g_nav`) | `actuatorWriteCommand(uint16)` |
| Rueckgabe | `bool` (Erfolg) | `float` (Winkel) | `void` |
| State-Schreib | IMU-Felder in `g_nav` | WAS-Felder in `g_nav` | keine |
| Feature-Gate | keins | keins | keins |
| Health-Check | implizit (`quality_ok`) | implizit (`quality_ok`) | keiner |

---

## 5. Identifizierte Probleme (7 Probleme, priorisiert)

### P1 — HAL Init feature-blind [HOCH]

**Datei:** `src/hal_esp32/hal_impl.cpp` -> `hal_esp32_init_all()`

**Problem:** Die Funktion initialisiert SAEMTLICHE Hardware-Schnittstellen, unabhaengig davon ob die entsprechenden Feature-Flags gesetzt sind.

**Auswirkung:**
- Unnoetige SPI-Zeit, GPIO-Pins, Speicher-Verbrauch
- Fehlhardware auf deaktivierten Bussen kann Start verzoegern
- Verwirrende Log-Meldungen beim Start fuer deaktivierte Features

### P2 — Logic-Init orphaned [MITTEL]

**Dateien:** `imu.cpp`, `steer_angle.cpp`, `actuator.cpp`

**Problem:** Drei Module definieren eigene Init-Funktionen (`imuInit()`, `steerAngleInit()`, `actuatorInit()`), die NIEMALS aufgerufen werden.

### P3 — Kein einheitliches Modul-Interface [HOCH]

**Alle Module**

**Problem:** Jedes Modul hat ein anderes API-Pattern. Keine standardisierten Lebenszyklus-Funktionen. Benennung inkonsistent. Rueckgabetypen variieren. Ort der State-Schreibung uneinheitlich.

### P4 — Doppelte State-Schreibungen [MITTEL]

**Datei:** `steer_angle.cpp` + `control.cpp`

**Problem:** `steerAngleReadDeg()` schreibt `g_nav.steer_angle_deg`. `controlStep()` schreibt es nochmal.

### P5 — Monolithischer Global State [MITTEL]

**Datei:** `src/logic/global_state.h`

**Problem:** `NavigationState` enthaelt ueber 30 Felder mit unterschiedlichen fachlichen Verantwortlichkeiten in einem einzigen Mutex-geschuetzten Bereich.

### P6 — controlStep() buendelt alles [HOCH]

**Datei:** `src/logic/control.cpp` -> `controlStep()`

**Problem:** Safety-Input, IMU, WAS, PID, Aktor in EINER Funktion. Weder isoliert testbar noch an-/abschaltbar.

### P7 — RTCM ohne Feature-Gate [NIEDRIG]

**Datei:** `src/logic/net.cpp`

**Problem:** `netPollRtcmReceiveAndForward()` laeuft immer, obwohl `FEAT_GNSS` default 0 ist.

---

## 6. Refactoring-Vorschlaege

### 6.1 Einheitliches Modul-Interface (loest P2, P3)

Jedes Hardware-Modul MUSS vier Funktionen bereitstellen:

| Phase | Funktion | Signatur | Beschreibung |
|-------|----------|----------|-------------|
| Feature-Check | `xxxIsEnabled()` | `constexpr bool -> bool` | Compile-Zeit Pruefung |
| Init | `xxxInit()` | `void -> void` | Einmalige Initialisierung |
| Update | `xxxUpdate()` | `void -> bool` | Input lesen, Plausibilitaet, State schreiben |
| Health | `xxxIsHealthy()` | `uint32_t -> bool` | Freshness-Timeout-Gesundheitscheck |

### 6.2 Modul-Registrierung und Iteration (loest P6)

```cpp
struct ModuleOps {
    const char* name;
    bool (*isEnabled)();
    void (*init)();
    bool (*update)();
    bool (*isHealthy)(uint32_t now_ms);
};
```

### 6.3 Feature-Gates konsequent anwenden (loest P1, P7)

- `#if`-Guards um HAL-Init-Aufrufe in `hal_esp32_init_all()`
- `#if`-Guards um Modul-Aufrufe in `controlStep()`
- `#if FEAT_GNSS` um RTCM-Pfad in `net.cpp`

### 6.4 State pro Modul kapseln (loest P4, P5)

Sub-Structs: `ImuState`, `SteerState`, `SwitchState`, `PidState`, `GnssState`, `WatchdogState`

### 6.5 controlStep() entflechten (loest P6)

Phasen: `controlReadSafety()` -> `controlReadSensors()` -> `controlCheckWatchdog()` -> `controlComputePid()` -> `controlWriteActuator()` -> `controlWriteState()`

---

## 7. Umsetzungsreihenfolge (empfohlen)

### Phase 1: Feature-Gates (P1, P2, P7)
1. `hal_esp32_init_all()` mit `#if`-Guards versehen
2. `controlStep()` mit `feat::`-Guards versehen
3. RTCM-Pfad in `net.cpp` mit `#if FEAT_GNSS` gardieren
4. Totcode entfernen: `imuInit()`, `steerAngleInit()`, `actuatorInit()` loeschen

### Phase 2: Einheitliches Modul-Interface (P3)
1. Namenskonvention einfuehren
2. IMU, WAS, Aktor umbenennen
3. `ModuleOps`-Registry einfuehren
4. `controlStep()` auf Registry-basierte Iteration umstellen

### Phase 3: State-Kapselung (P4, P5)
1. Sub-Structs definieren
2. `NavigationState` aus Sub-Structs zusammensetzen
3. Ownership-Regeln durchsetzen
4. WAS doppelte State-Schreibung entfernen

### Phase 4: controlStep entflechten (P6)
1. Phasen-Funktionen extrahieren
2. `controlStep()` als Orchestrator

---

## 8. Sicherheitshinweise fuer Agenten

### 8.1 HAL-Schnittstelle
`hal.h` ist das stabilste Element. Neue Funktionen hinzufuegen ist OK, bestehende Signaturen aendern nur mit Pruefung aller Aufrufer.

### 8.2 Shared State Mutex-Korrektheit
JEDE Aenderung an `g_nav` MUSS durch `StateLock lock;` gesichert werden.

### 8.3 Timing-Kritikalitaet
- `controlTask`: 200 Hz (5 ms). Keine blockierenden Operationen!
- `commTask`: 100 Hz (10 ms).
- SD-Zugriffe laufen asynchron ueber PSRAM Ring-Buffer.

### 8.4 SPI2_HOST Shared Bus
IMU (BNO085), WAS (ADS1118) und Aktor teilen sich SPI2_HOST. Aenderungen an SPI-Zugriffsreihenfolge koennen Timing-Probleme verursachen.

### 8.5 Safety-Schaltung
`hal_safety_ok()` MUSS in jedem `controlStep()` aufgerufen werden. Wenn Safety LOW -> Aktor sofort stoppen. Darf nie durch Feature-Gates blockiert werden.

### 8.6 Compile-Zeit-Guards bevorzugen
Feature-Gates als `#if` (Compile-Zeit), nicht als `if` (Laufzeit). Spart Flash-Speicher.

### 8.7 AgOpenGPS PGN-Kompatibilitaet
PGN 253, 250, 214 muessen byte-genau dem AgOpenGPS-Protokoll entsprechen.
Referenz: https://github.com/AgOpenGPS-Official/Boards/blob/main/PGN.md

### 8.8 PlatformIO Build-Konfiguration
Feature-Flags werden ueber `platformio.ini` mit `-D` Flags gesetzt.

---

## 9. Zusammenfassung der Kernbotschaft

Die ZAI_GPS Firmware hat eine solide architektonische Basis (PLC-Dreischritt, HAL, Global State, Dual-Core), aber die Konsistenz der Modul-Behandlung laesst zu wuenschen uebrig. Die sieben identifizierten Probleme (P1-P7) lassen sich auf drei Kernursachen zurueckfuehren:

1. **Feature-Flags greifen nicht durchgehend** — HAL-Init, Logic-Update und RTCM-Pfad ignorieren Flags
2. **Kein einheitlicher Modul-Contract** — jedes Modul hat eigene Namenskonventionen, Rueckgabetypen und Lebenszyklus-Verhalten
3. **Monolithische Funktion und monolithischer State** — `controlStep()` und `NavigationState` buendeln zu viele Verantwortlichkeiten
