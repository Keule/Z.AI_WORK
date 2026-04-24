# Architektur-Analyse V2 — ZAI_GPS Firmware

**Datum**: 2026-04-24
**Basis-Commit**: c3c61be (+ Step 1 Merge dc0e151)
**Agent**: Architecture-Improvement-Agent
**Branch**: plan/architecture-improvement

---

## 1. Einleitung

### Zweck

Diese Architektur-Analyse V2 dokumentiert den aktuellen Stand der ZAI_GPS (AgSteer) Firmware
nach Abschluss von Schritt 1 des Architektur-Verbesserungsplans. Sie dient als fundierte
Entscheidungsgrundlage für die verbleibenden Implementierungsschritte (Schritt 3–7) und
als Referenz für zukünftige Architektur-Entscheidungen. Im Vergleich zur V1-Analyse wurden
zusätzliche ADRs (ADR-005, ADR-006) erstellt, die StateLock-Migration abgeschlossen und der
Codebasis-Stand aktualisiert.

Die Analyse umfasst eine detaillierte Untersuchung der dreischichtigen Architektur, des
Task-Modells, des State-Managements, des Feature- und Modulsystems, der Kommunikationsarchitektur
sowie eine Identifikation bestehender Architekturprobleme mit priorisierten Lösungsvorschlägen.
Besonderes Augenmerk liegt auf der Operationalisierbarkeit des neuen Betriebsmodus-Konzepts
(ADR-005) und des Konfigurations-Frameworks (ADR-006).

### Methode

Die Analyse wurde durch statische Code-Analyse der Quelldateien unter `src/` und `include/`
durchgeführt. Folgende Methoden kamen zur Anwendung:

- Zeilenweise Code-Review aller Kernmodule (main.cpp, control.cpp, net.cpp, modules.cpp, cli.cpp, hal_impl.cpp)
- Trace der Task-Architektur und FreeRTOS-Synchronisationsmechanismen
- Analyse der StateLock-Nutzungsmuster und Writer-Ownership-Regeln
- Bewertung der Feature-Gates und Compile-Time-Konfiguration
- Review aller bestehenden Architecture Decision Records (ADRs)
- Groessen- und Komplexitaetsanalyse pro Datei und Modul

### Basis-Commit und Vergleich mit V1

- **Basis**: c3c61be (development HEAD vor Architektur-Branch) + dc0e151 (Step 1 Bereinigungen)
- **Vergleich mit V1**: Die V1-Analyse wurde vor dem Architektur-Verbesserungsplan erstellt.
  Seitdem wurden folgende Veraenderungen vorgenommen:
  - ADR-STATE-001 Migration: desiredSteerAngleDeg nach NavigationState verschoben
  - OperatingMode State Machine (op_mode.h/cpp) implementiert
  - Config Framework (config_*.h/cpp) erstellt mit 4 Kategorien
  - Erweiterte Diagnose (diagnostics.h/cpp)
  - UM980 UART Rollen-Konfiguration
  - TASK-040 Bereinigung abgeschlossen

---

## 2. Systemübersicht

### 2.1 Drei-Schichten-Architektur

Die Firmware folgt einer klaren dreischichtigen Architektur, die Hardware-Abhaengigkeiten
von der Geschaeftslogik trennt. Diese Trennung ermoeglicht sowohl Portabilitaet auf andere
MCU-Plattformen als auch testbare Geschaeftslogik.

```
┌────────────────────────────────────────────────────────────────────┐
│  Main Layer (main.cpp) — Arduino setup()/loop()                     │
│  - Task-Erstellung und Boot-Sequenz                               │
│  - Module-Aktivierung und Pin-Claim-Arbitrierung                  │
│  - Betriebsmodus-Initialisierung (ADR-005)                         │
│  - Telemetrie, Setup-Wizard, CLI-Bridge                            │
├────────────────────────────────────────────────────────────────────┤
│  Logic Layer (src/logic/) — Plattformunabhaengig                   │
│  - PGN-Codec (Encode/Decode des AgOpenGPS-Protokolls)             │
│  - Control (PID-Regler, Safety-Pipeline, Sensor-Fusion)           │
│  - Net (UDP-Kommunikation, RTCM-Forwarding)                       │
│  - NTRIP (TCP-Client fuer RTK-Korrekturdaten)                     │
│  - Modules (Feature-Registry, Hardware-Detection, Pin-Claims)     │
│  - CLI (Serial Command Interface, Config Menu)                    │
│  - State (NavigationState, StateLock, Sub-Structs)                │
│  - Logging (ESP-Log-Erweiterung, Tag-Registry, Filter)           │
│  - Config Framework (Runtime-Konfiguration, Validierung)          │
│  - Diagnostics (Selftest, Pin-Map, Module-Status)                 │
├────────────────────────────────────────────────────────────────────┤
│  HAL Layer (src/hal/ + src/hal_esp32/)                             │
│  - hal.h: Oeffentliche C-Schnittstelle (plattformunabhaengig)     │
│  - hal_impl.cpp: ESP32-S3 Implementierung                         │
│    - SPI Bus Management (Sensor-Bus SPI2_HOST)                    │
│    - W5500 Ethernet (ESP-IDF ETH-Treiber)                         │
│    - ADS1118 ADC (Steer Angle Sensor)                             │
│    - BNO085 IMU (Heading, Roll, Yaw-Rate)                         │
│    - Actuator (SPI-PWM-Driver)                                    │
│    - GNSS UART (UM980 RTCM-Output)                                │
│    - TCP Client (NTRIP-Verbindung)                                │
│    - Pin Claim Arbitration (ADR-HAL-001)                          │
│    - Mutex (FreeRTOS Recursive Mutex fuer NavigationState)        │
│    - SD Card (One-Shot Boot-Probe)                                │
└────────────────────────────────────────────────────────────────────┘
```

### 2.2 Schichtengrenzen und Abhaengigkeiten

Die Schichtengrenzen sind klar definiert durch die HAL-Schnittstelle in `hal.h`. Die
Logic-Schicht greift ausschliesslich ueber die in `hal.h` deklarierten Funktionen auf die
Hardware zu. Keine Logic-Datei inkludiert ESP32-spezifische Header direkt; die HAL-Abstraktion
wird ueber `hal_esp32/hal_impl.h` und `hal_esp32/hal_impl.cpp` realisiert. Diese saubere
Trennung ermoeglicht theoretisch einen Platform-Port (z.B. STM32), indem nur die
HAL-Implementierung ausgetauscht wird.

Die Main-Schicht dient als Orchestrations-Layer und enthaelt keine Geschaeftslogik. Sie ist
verantwortlich fuer die Boot-Sequenz, Task-Erstellung, Module-Aktivierung und den Ueberblick
ueber das Gesamtsystem. Die Logic-Schicht ist die eigentliche Domänen-Schicht mit allen
algorithmischen und protokollspezifischen Implementierungen.

---

## 3. Task-Modell

### 3.1 Übersicht

Die Firmware nutzt das FreeRTOS-Betriebssystem des ESP32 und implementiert drei dedizierte
Tasks plus den Arduino `loop()` als vierte Ausfuehrungseinheit. Die Task-Architektur
orientiert sich an ADR-002 (Task-Modell) und priorisiert Echtzeitfaehigkeit der Regelung
ueber alle anderen Operationen.

| Task | Core | Priorität | Frequenz | Stack | Verantwortung |
|------|------|-----------|----------|-------|--------------|
| controlTask | 1 | configMAX_PRIORITIES-2 | 200 Hz (5 ms) | 4096 | PID, Safety, Sensor-Read, Actuator |
| commTask | 0 | configMAX_PRIORITIES-3 | 100 Hz (10 ms) | 4096 | UDP/PGN, RTCM, HW-Status |
| maintTask | 0 | 1 (niedrigste) | 1-2 Hz | 4096 | SD-Flush, NTRIP-Tick, ETH-Monitor |
| loop() | 1 | niedrig | ~10 Hz | Arduino | CLI, Telemetrie, Wizard |

### 3.2 controlTask (Core 1, 200 Hz)

Der controlTask ist der zeitkritischste Task der gesamten Firmware. Er laeuft auf Core 1
mit zweithoechster Prioritaet und realisiert eine strikte 5 ms-Zykluszeit mittels
`vTaskDelayUntil()` fuer Jitter-Minimierung. Die Pipeline wurde in `controlStep()`
modularisiert und fuehrt folgende Phasen aus:

1. **Safety-Check** (immer): Liest den Safety-Pin via `hal_safety_ok()` und aktualisiert
   `g_nav.safety.safety_ok` unter StateLock. Bei Safety-KICK wird die Regelung sofort
   deaktiviert und der Aktuator auf 0 gesetzt.
2. **Sensor-Read**: Liest IMU und WAS (Wheel Angle Sensor) ueber die `ModuleOps`-Schnittstelle.
   Die Sensor-Daten werden mit Feature-Gates (`FEAT_ENABLED`) abgesichert.
3. **AgIO-Snapshot**: Liest alle relevanten Eingaben aus `NavigationState` (Work-Switch,
   Steer-Switch, GPS-Speed, Watchdog, desiredSteerAngleDeg) atomar unter einem
   StateLock-Block. Dies garantiert konsistente Eingabedaten fuer den PID-Regler.
4. **Watchdog-Check**: Prueft ob der Watchdog-Timer (von AgIO gesendet) abgelaufen ist.
   Bei Timeout wird die Regelung deaktiviert.
5. **PID-Berechnung**: Berechnet den Aktuator-Befehl basierend auf Soll-Ist-Vergleich.
   Fehler-Wrapping auf ±180°, Anti-Windup, derivativer Anteil auf Fehler.
6. **Actuator-Write + State-Write**: Schreibt den PWM-Befehl an den Aktuator und
   aktualisiert g_nav unter StateLock.

Der Task fuehrt die Pipeline nur im ACTIVE-Modus aus (ADR-005: `opModeIsControlActive()`).
Im PAUSED-Modus wird nur der Watchdog-Feed aufrechterhalten. SD-Logger-Aufzeichnung
erfolgt mit 10 Hz (alle 20 Zyklen), um SPI-Bus-Konflikte zu minimieren.

### 3.3 commTask (Core 0, 100 Hz)

Der commTask verarbeitet die gesamte Netzwerkkommunikation mit AgIO und GNSS-Empfaengern.
Er laeuft auf Core 0 mit dritthoechster Prioritaet und pollt mit 10 ms-Intervall.

**Input-Phase**: `netPollReceive()` liest UDP-Datagramme von allen konfigurierten Sockets
(AgIO-Port 8888, RTCM-Port). RTCM-Daten werden in einen 4096-Byte-Ringpuffer geschrieben
und via `netPollRtcmReceiveAndForward()` an den GNSS-UART weitergeleitet.

**Processing-Phase**: `modulesUpdateStatus()` ueberwacht dynamische Hardware-Aenderungen
(insbesondere den Safety-Zustand).

**Output-Phase**: `netSendAogFrames()` sendet PGN 253 (SteerStatus), PGN 250 (FromAutosteer2)
und PGN 214 (GpsMainOut) an AgIO mit 10 Hz. Die Ausgabedaten werden atomar aus
`NavigationState` gelesen (Snapshot-Pattern) und ausserhalb des Locks encodiert und gesendet.

**Monitoring**: Hardware-Status wird mit 1 Hz aktualisiert via `hwStatusUpdate()`, das
Fehlerzaehler fuer Ethernet, Safety, Steer-Angle, IMU, NTRIP und Modul-Aktivitaet fuehrt.

Auch der commTask fuehrt seine Pipeline nur im ACTIVE-Modus aus (`opModeIsCommActive()`).

### 3.4 maintTask (Core 0, niedrigste Prioritaet)

Der maintTask wurde mit TASK-029 eingefuehrt, um blockierende Operationen vom commTask
zu trennen (ADR-002). Er laeuft auf Core 0 mit niedrigster Prioritaet (1) und fuehrt aus:

- **ETH Link Monitoring** (1 Hz): Ueberwacht den Ethernet-Link-Zustand
- **NTRIP State Machine** (1 Hz): TCP-Connect, Authentication, RTCM-Stream-Pflege.
  Enthaelt blocking TCP-Connect mit 5s Timeout (in commTask nicht zulaessig)
- **SD Logger Flush** (2 Hz): Schreibt den PSRAM-Ringbuffer auf die SD-Karte
- **Switch Debounce + Log Toggle**: GPIO-Entprellung fuer Logging-Schalter
- **OpMode GPIO Poll** (100 ms): Prueft den konfigurierbaren Modus-Umschalter-Pin

Der maintTask ist der einzige Task, der blocking Operationen ausfuehren darf, da seine
niedrige Prioritaet weder die Echtzeitaufgaben auf Core 1 noch die commTask-Polling-Frequenz
auf Core 0 beeintraechtigt.

### 3.5 loop() (Core 1, niedrige Prioritaet)

Die Arduino `loop()`-Funktion laeuft auf Core 1 mit der Standard-Arduino-Prioritaet. Sie
dient als Ergaenzung zu den FreeRTOS-Tasks:

- Watchdog-Feed fuer beide Tasks
- Telemetrie-Logging (1 Hz): System-Status auf Serial
- Setup-Wizard: Interaktive Erstkonfiguration (nur im PAUSED-Modus)
- CLI-Processing: Serial-Eingabe an `cliProcessLine()` weiterleiten
- PID Live Mode: Echtzeit-PID-Anzeige (nur im ACTIVE-Modus)

---

## 4. State Management

### 4.1 NavigationState mit Sub-Structs

Die gesamte Laufzeit状态 der Firmware wird in der zentralen Struktur `NavigationState`
zusammengefasst, die in `global_state.h` definiert ist. Sie besteht aus sechs fachlich
getrennten Sub-Structs, die jeweils einen klar definierten Writer-Task haben:

| Sub-Struct | Writer-Task | Reader-Tasks | Lock |
|-----------|-------------|-------------|------|
| `ImuState` | imu.cpp | net.cpp, sd_logger, hw_status | StateLock |
| `SteerState` | control.cpp (ONLY) | net.cpp, sd_logger, modules | StateLock |
| `SwitchState` | net.cpp (PGN 254) | control.cpp, modules | StateLock |
| `PidConfigState` | control.cpp, net.cpp | net.cpp, modules | StateLock |
| `SafetyState` | control.cpp | net.cpp, modules, hw_status | StateLock |
| `GnssState` | net.cpp | net.cpp, main.cpp | StateLock |

Jedes Feld hat genau einen dedizierten Writer. Diese Single-Writer-Regel eliminiert
Write-Write-Konflikte und vereinfacht die Korrektheitsanalyse erheblich.

### 4.2 StateLock-Pattern

Der StateLock ist ein RAII-Wrapper um den FreeRTOS Recursive Mutex:

```cpp
{
    StateLock lock;  // ctor: hal_mutex_lock() / dtor: hal_mutex_unlock()
    g_nav.xxx.yyy = value;  // sicherer Zugriff
}
```

Alle Zugriffe auf `g_nav` ausserhalb des controlTask (der alleiniger Writer fuer
SteerState und SafetyState ist) muessen unter StateLock erfolgen. Der Recursive Mutex
erlaubt geschachtelte Locks innerhalb desselben Tasks (z.B. in `controlStep()`, das
mehrmals lesend und schreibend auf verschiedene Sub-Structs zugreift).

### 4.3 ADR-STATE-001 Umsetzung

ADR-STATE-001 wurde in Schritt 1c vollstaendig umgesetzt. `desiredSteerAngleDeg` wurde
von einer globalen `extern volatile float` nach `g_nav.sw.desiredSteerAngleDeg` in
`SwitchState` verschoben. Alle vier Zugriffsstellen nutzen nun StateLock:

- **net.cpp** (Writer): Schreibt den Sollwinkel aus PGN 254 unter StateLock (Zeile 371-377)
- **control.cpp** (Reader): Liest den Sollwinkel fuer PID-Berechnung unter StateLock (Zeile 354-359)
- **sd_logger.cpp** (Reader): Liest den Sollwinkel fuer Telemetrie-Aufzeichnung unter StateLock
- **main.cpp** (Reader): Liest den Sollwinkel in loop()-Telemetrie unter StateLock

Ein Bugfix in Schritt 1 korrigierte einen Race-Condition-Fehler in control.cpp, bei dem
der Lesezugriff ausserhalb des StateLock-Blocks erfolgte.

### 4.4 Snapshot-Pattern fuer konsequente Reads

Fuer Reader, die mehrere Felder konsistent lesen muessen (z.B. commTask fuer PGN 253
Encoding), wird das Snapshot-Pattern verwendet: Alle benoetigten Felder werden in einem
einzelnen StateLock-Block in eine lokale Struktur kopiert, die anschliessend ohne Lock
verarbeitet wird. Dies minimiert die Lock-Haltedauer und vermeidet Deadlocks.

---

## 5. Feature- und Modulsystem

### 5.1 Compile-Time Features (feat::)

Die Firmware nutzt ein zweistufiges Feature-Gating-System. Build-Flags in platformio.ini
definieren `-DFEAT_XXX`, die in `features.h` in `FEAT_COMPILED_XXX` Konstanten umgesetzt
werden. Der `feat::`-Namespace stellt Inline-constexpr-Checks bereit:

```cpp
namespace feat {
    inline constexpr bool imu()    { return FEAT_ENABLED(FEAT_COMPILED_IMU); }
    inline constexpr bool ads()    { return FEAT_ENABLED(FEAT_COMPILED_ADS); }
    // ... 9 Features insgesamt
}
```

Die Features sind: IMU, ADS, ACT, ETH, GNSS, NTRIP, SD, SAFETY, LOGSW. ETH ist
Pflicht (`static_assert(FEAT_COMPILED_ETH)`). Profile in platformio.ini definieren
die Kombination: `profile_full_steer_ntrip` aktiviert alle Features.

### 5.2 Runtime Module System (ModuleOps)

Das Runtime-Modulsystem implementiert eine dreiwertige Zustandsmaschine pro Modul:
`MOD_UNAVAILABLE → MOD_OFF → MOD_ON`. Es verwaltet 9 Feature-Module mit folgenden
Funktionen:

- `moduleActivate(id)`: Prueft Compile-Time-Flag, Dependencies und Pin-Claim-Konflikte
- `moduleDeactivate(id)`: Gibt Pin-Claims frei
- `moduleIsActive(id)`: Prueft ob state == MOD_ON
- `moduleGetInfo(id)`: Liefert Descriptor (Name, Pins, Dependencies, hw_detected)

Die Modul-Aktivierung in `setup()` folgt einer strikten Reihenfolge, die Dependencies
beruecksichtigt: IMU, ADS, ETH, GNSS, SAFETY (keine Dependencies) → ACT (braucht IMU+ADS)
→ SD (bedingt) → NTRIP (braucht ETH).

### 5.3 Pin-Claim-Arbitrierung (ADR-HAL-001)

Das Modulsystem nutzt die HAL-Pin-Claim-Arbitrierung, um GPIO-Konflikte zu verhindern.
Jedes Modul deklariert seine Pins im Board-Profil (`fw_config.h` → `board_profile_select.h`).
Bei `moduleActivate()` werden alle Pins geclaimt; bei Konflikten schlaegt die Aktivierung
fehl mit Rollback. Die Pin-Claim-Tabelle hat eine Kapazitaet von 32 Eintraegen.

### 5.4 Mod-State-Maschine

```
MOD_UNAVAILABLE → MOD_OFF → MOD_ON
                  ↑_________|
```

- `MOD_UNAVAILABLE`: Feature nicht kompiliert oder keine Pins verfuegbar
- `MOD_OFF`: Kompiliert und verfuegbar, aber nicht aktiv (Pins nicht geclaimt)
- `MOD_ON`: Kompiliert, verfuegbar und aktiv (Pins geclaimt, Hardware initialisiert)

`modulesInit()` setzt die `compiled`- und `hw_detected`-Flags basierend auf
Hardware-Detection. Default-Zustaende fuer NTRIP, LOGSW und SD werden aus
`soft_config.h` (`cfg::`-Namespace) gelesen.

---

## 6. Modul-Inventory

| Modul | Datei(en) | Interface (Pflicht) | Writer-Ownership | Dependencies | Status | Zeilen |
|-------|-----------|---------------------|-----------------|--------------|--------|--------|
| main.cpp | main.cpp | Arduino setup/loop | alle Module (Orchestrator) | alle Module | aktiv | 1186 |
| HAL | hal_impl.cpp | hal.h (62 Funktionen) | — | SPI, ETH, GPIO, ADC, TCP | Monolith | 2099 |
| Control | control.cpp, control.h | controlInit/Step, pidCompute | SteerState, SafetyState, PidConfigState | IMU, ADS, ACT | stabil | 385+137 |
| Network | net.cpp, net.h | netInit, netPollReceive, netSendAogFrames | SwitchState (PGN 254), PidConfigState (PGN 251), GnssState | hal_net, PGN-Codec | stabil | 597+47 |
| Modules | modules.cpp, modules.h | modulesInit, moduleActivate | — | Feature-Flags, Pin-Claims | stabil | 578+185 |
| CLI | cli.cpp, cli.h | cliInit, cliProcessLine, cliRegisterCommand | — | alle Module | aktiv | 861+31 |
| NTRIP | ntrip.cpp, ntrip.h | ntripInit, ntripTick, ntripReadRtcm | NtripState | hal_tcp | stabil | 548+63 |
| IMU | imu.cpp, imu.h, hal_bno085.cpp | imuInit, imuUpdate | ImuState | hal_imu (SPI) | stabil | 84+29+450 |
| WAS | was.cpp, was.h | wasInit, wasUpdate, wasGetAngleDeg | — (Cache, kein g_nav Writer) | hal_steer_angle | stabil | 79+39 |
| Actuator | actuator.cpp, actuator.h | actuatorInit, actuatorUpdate | — | hal_actuator (SPI) | stabil | 42+31 |
| SD Logger | sd_logger.cpp, sd_logger_esp32.cpp | sdLoggerInit, sdLoggerRecord | — | SD, StateLock, PSRAM | stabil | 226+133+582 |
| HW Status | hw_status.cpp, hw_status.h | hwStatusInit, hwStatusUpdate | — | StateLock, Modules | stabil | 300+138 |
| Runtime Config | runtime_config.cpp, nvs_config.cpp | softConfigGet, nvsConfigSave | — | NVS | stabil | 190+49 |
| PGN Codec | pgn_codec.cpp, pgn_types.h | pgnEncode*, pgnDecode* | — | — | stabil | 519+368 |
| Config Framework | config_framework.cpp, config_menu.cpp | configFrameworkInit, configMenuShow | — | RuntimeConfig, NVS | neu (Step 5) | — |
| OpMode | op_mode.cpp, op_mode.h | opModeInit, opModeRequest, opModeGet | — | — | neu (Step 3) | — |
| Diagnostics | diagnostics.cpp, diagnostics.h | diagRunSelftest, diagPrintModuleStatus | — | Modules, HAL | neu (Step 6) | — |
| Setup Wizard | setup_wizard.cpp, setup_wizard.h | setupWizardRequestStart | — | CLI, NVS | aktiv | 139+16 |
| UM980 Setup | um980_uart_setup.cpp, um980_uart_setup.h | um980SetupApply, um980SetupSetRole | — | hal_gnss | aktiv | 173+45 |
| Diag | diag.cpp, diag.h | diagPrintHw, diagPrintMem, diagPrintNet | — | hal_net, ESP | stabil | 66+15 |

**Gesamt**: ~6.548 Zeilen C++ in den Hauptquelldateien (ohne Lib/), ~3.500 Zeilen Header

---

## 7. Kommunikationsarchitektur

### 7.1 UDP/PGN-Protokoll

Die Firmware kommuniziert mit AgOpenGPS (AgIO) ueber UDP auf Basis des AOG-Ethernet-Protokolls.
Die Implementierung folgt dem offiziellen PGN-Standard und unterstuetzt folgende PGNs:

**Eingang (von AgIO):**
- PGN 200 (Hello From AgIO): Triggert Hello-Reply fuer alle aktiven Module
- PGN 202 (Scan Request): Triggert Subnet-Reply fuer alle aktiven Module
- PGN 201 (Subnet Change): Aktualisiert die Ziel-IP (Subnet-Aenderung)
- PGN 254 (Steer Data In): Sollwinkel, Geschwindigkeit, Switch-Status, Watchdog
- PGN 252 (Steer Settings In): PID-Parameter (Kp, PWM-Limits, Counts)
- PGN 251 (Steer Config In): Hardware-Konfigurations-Bits (ADR-PGN-001)
- PGN 221 (Hardware Message): Diagnose-Kommandos von AgIO

**Ausgang (an AgIO):**
- PGN 253 (Steer Status Out): Lenkwinkel, Heading, Roll, Switch-Status, PWM @ 10 Hz
- PGN 250 (From Autosteer 2): Sensor-Byte @ 10 Hz
- PGN 214 (Gps Main Out): Heading, Speed, Roll, Fix-Quality, Diff-Age @ 10 Hz

Nur Frames mit Source ID 0x7F (AgIO) werden verarbeitet; eigene Echo-Frames (0x7E) werden
ignoriert. RTCM-Daten werden auf einem separaten UDP-Socket empfangen (Dedicated RTCM Port).

### 7.2 RTCM-Pipeline

RTCM-Korrekturdaten durchlaufen folgende Pipeline:

1. **UDP-Empfang**: `netPollRtcmReceiveAndForward()` liest RTCM-Datagramme vom dedizierten
   UDP-Socket in einen 4096-Byte-Ringpuffer (`s_rtcm_ring`)
2. **Forwarding**: Daten werden aus dem Ringpuffer gelesen und via `hal_gnss_rtcm_write()`
   an den GNSS-UART (UM980) gesendet. Teilweise Writes werden akzeptiert.
3. **NTRIP-Alternative**: RTCM-Daten koennen auch vom NTRIP-TCP-Client kommen
   (`ntripReadRtcm()`), der vom maintTask bedient wird.

Bei Multi-Receiver-Betrieb (Dual-UM980) besteht ein acknowledged Risiko: Der aktuelle
Pop-first-Mechanismus kann bei mehreren Empfaengern zu asymmetrischer RTCM-Verteilung
fuehren (ADR-NTRIP-002). Peek-then-Pop-Verteilung ist fuer produktive Multi-Receiver-Nutzung
geplant.

### 7.3 NTRIP-Client

Der NTRIP-Client implementiert eine vollstaendige TCP-basierte Verbindung zu einem
RTK-Daten-Server:

- **State Machine**: IDLE → CONNECTING → AUTHENTICATING → CONNECTED → ERROR → DISCONNECTED
- **Reconnect**: Automatischer Reconnect mit 5s Verzoegerung nach Fehler
- **Blocking Connect**: TCP-Connect mit 5s Timeout, NUR im maintTask erlaubt
- **Authentifizierung**: Base64-encoded Username/Password
- **Rate Limiting**: RTCM-Drop-Zaehler fuer UART-Puffer-Ueberlauf

### 7.4 AgOpenGPS-Integration

Die Firmware fungiert als vollstaendiger AgOpenGPS-Steering-Controller und implementiert
drei AOG-Module:

- **Steer** (Src=0x7E, Port=5126): Lenkungsmodul mit Lenkwinkel, Heading und PID-Output
- **GPS** (Src=0x7C, Port=5124): GPS-Modul mit IMU-Heading und Roll
- **Machine** (Src=0x7B, Port=5127): Maschinenmodul (Platzhalter)

Die Hardware-Detection bestimmt, welche Module als "aktiv" gemeldet werden. AgIO entdeckt
Module via Hello (PGN 200) und Scan (PGN 202) Requests.

---

## 8. Identifizierte Architekturprobleme

### 8.1 main.cpp Komplexität (1186 Zeilen)

Trotz Codex-Merge und TASK-040-Bereinigung bleibt main.cpp die zweitgroesste Datei.
Folgende Probleme bestehen:

- **Duplicate Boot-CLI-Sessions**: `bootMaintRunCliSession()` (Zeile 136) und
  `runBootCliSession()` (Zeile 347) sind nahezu identisch — beide implementieren
  eine CLI-Schleife mit Serial, BT SPP und Web OTA. Dies ist ein Merge-Artefakt
  und sollte dedupliziert werden.
- **Duplicate Web-OTA-Handler**: `bootMaintWebHandleRoot/Update/Upload` (Zeilen 224-270)
  und `bootWebHandleRoot/Update/Upload` (Zeilen 435-481) sind funktional identisch.
- **Duplicate Service-Funktionen**: `bootMaintStartServices/StopServices` (Zeilen 272-340)
  und `startBootMaintenanceServices/StopServices` (Zeilen 483-551) sind identisch.
- **Monolithische setup()-Funktion**: Die setup()-Funktion umfasst ueber 200 Zeilen
  mit sequentieller Initialisierung von NVS, HAL, Modulen, Config, NTRIP, OpMode,
  Kalibrierung und Task-Erstellung.

### 8.2 hal_impl.cpp Monolith (2099 Zeilen)

hal_impl.cpp ist die groesste Datei im Projekt und buendelt mehrere fachliche Domänen:
SPI-Bus-Management (Mutex, Telemetrie, Client-Switching), W5500 Ethernet (ETH-Treiber,
UDP-Sockets, IP-Konfiguration), ADS1118 ADC (libdriver-Integration), GPIO/Safety,
Pin-Claim-Arbitrierung (32-Eintrag-Tabelle), TCP-Client (NTRIP), GNSS-UART,
und Logging (hal_log mit Tag-Parsing). ADR-HAL-002 definiert die Aufteilung in
fünfd Domänenmodule (hal_init.cpp, hal_eth.cpp, hal_spi.cpp, hal_ads1118.cpp, hal_gpio.cpp),
die in Schritt 7 umgesetzt wird.

### 8.3 cli.cpp Monolith (861 Zeilen)

cli.cpp enthaelt alle CLI-Handler als statische Funktionen in einem anonymen Namespace.
Mit 861 Zeilen und 19 registrierten Kommandos wird die Datei zunehmend unübersichtlich.
Die Handler decken sechs verschiedene Funktionsbereiche ab (System, NTRIP, PID, Netzwerk,
Modul, Aktuator, Diagnose, UART, OpMode, Config), die nicht klar separiert sind.
Eine Modularisierung nach Themenbereichen (z.B. cli_ntrip.cpp, cli_net.cpp, cli_config.cpp)
wuerde die Wartbarkeit deutlich verbessern. ADR-MODULE-001 fordert standardisierte
Modul-Schnittstellen, die auch auf die CLI-Handler anwendbar sind.

### 8.4 5 Proposed ADRs nicht finalisiert

Fünf ADRs haben den Status "proposed" und muessen in den naechsten Schritten finalisiert
werden:

| ADR | Thema | Schritt |
|-----|-------|---------|
| ADR-MODULE-001 | Module Interface Standardization | Schritt 7 |
| ADR-NTRIP-002 | Multi-Receiver RTCM Distribution | Schritt 6 |
| ADR-PGN-001 | PGN-251 Config to Hardware | Schritt 5 |
| ADR-LOG-002 | Tag Registry + Runtime Filter | Schritt 5 |
| ADR-HAL-002 | HAL-Impl Split | Schritt 7 |

ADR-STATE-001 wurde in Schritt 1c migriert und hat den Status "accepted".

### 8.5 Laufzeit Betriebsmodus-Wechsel teilweise implementiert

Das Betriebsmodus-Konzept (ADR-005) wurde in den Schritten 3 und 4 implementiert:
OpMode State Machine, Task-Awareness, Safety-Logik, AgIO-Benachrichtigung (PGN 253
PAUSED-Bit). Die Implementierung ist funktional, aber folgende Punkte bleiben offen:

- **WiFi/BT im PAUSED-Modus**: Die Boot-Maintenance-Services (WiFi AP, BT SPP, Web OTA)
  werden aktuell nur beim Boot gestartet. Ein Runtime-Wechsel zu PAUSED startet diese
  Services nicht automatisch.
- **GPIO Mode-Switch**: Die GPIO-Polling-Funktion existiert (`opModeGpioPoll()`) ist
  aber deaktiviert (`opModeSetGpioEnabled(false)`).
- **Web UI**: Noch nicht implementiert (zukuenftig geplant).

### 8.6 Fragmentierte Konfigurationsschnittstellen

Die Konfiguration erfolgt ueber multiple, inkonsistente Pfade:
- Serial CLI Einzelkommandos (`ntrip set`, `net mode`, `pid set`, etc.)
- Config Framework (`config` Kommando mit Kategorien)
- PGN 252 (PID-Settings von AgIO)
- PGN 251 (Hardware-Config von AgIO)
- NVS (Persistenz)
- SD-Override (`/ntrip.cfg`, `/config.ini`)
- Setup-Wizard (Erstkonfiguration)

ADR-006 definiert ein einheitliches Config-Framework, das in Schritt 5 teilweise
implementiert wurde (4 Kategorien: NTRIP, GNSS, Network, System). Die PGN-251/252
Integration und die vollstaendige Validierung sind noch offen.

---

## 9. Betriebsmodus-Konzept

Das Betriebsmodus-Konzept basiert auf ADR-005 und wurde in den Schritten 3 und 4
grundlegend implementiert. Die drei Zustaende der Zustandsmaschine sind:

```
BOOTING ──┬──→ ACTIVE  (safety HIGH nach Init)
          └──→ PAUSED  (safety LOW nach Init → Boot-Maintenance)

ACTIVE ──→ PAUSED  (safety LOW + speed < 0.1 km/h + keine SPI-Transaktion)
PAUSED ──→ ACTIVE  (Manuelles Trigger + ETH-Link-Check + Sanity-Check)
```

### 9.1 Task-Verhalten pro Modus

Im **ACTIVE**-Modus laufen alle Tasks vollumfänglich:
- controlTask@200Hz: Volle PID-Pipeline (Safety → Sensor → Watchdog → PID → Actuator)
- commTask@100Hz: UDP-Empfang, PGN-Verarbeitung, RTCM-Forwarding, Status-Meldungen
- maintTask@1Hz: SD-Flush, NTRIP-Tick, ETH-Monitor
- loop(): Telemetrie, CLI (eingeschraenkt), PID-Live-Anzeige

Im **PAUSED**-Modus wird die Echtzeit-Pipeline angehalten:
- controlTask: Nur Watchdog-Feed und SD-Logger (keine SPI-Sensor-Zugriffe)
- commTask: Nur Watchdog-Feed und HW-Status-Monitoring (kein UDP-Empfang/-Send)
- maintTask: Erweitert (zusaetzlich: WiFi AP, BT SPP, SD-Export, Diagnose)
- loop(): Volle CLI, Setup-Wizard, Selftest, Config Menu

### 9.2 Uebergangs-Sicherheit

Die Uebergangsbedingungen stellen sicher, dass kein Moduswechsel waehrend der
Fahrt oder waehrend kritischer Operationen erfolgt:

- **ACTIVE → PAUSED**: Safety-Schalter LOW (manuelle Bestätigung) UND
  Geschwindigkeit unter 0.1 km/h (MIN_STEER_SPEED_KMH Threshold). Dies verhindert
  versehentliches Umschalten waehrend der Autosteer-Operation.
- **PAUSED → ACTIVE**: Manuelle Ausloesung (CLI: `mode active`) mit anschliessendem
  Sanity-Check (ETH-Link, alle Module aktiv, keine Blocking-Transfers).

### 9.3 AgIO-Integration

AgIO wird über PGN 253 Status-Bit 6 (0x40) ueber den PAUSED-Zustand informiert.
Bei Moduswechsel wird das Hello-PGN erneut gesendet mit aktualisiertem Status.
PID-Output wird im PAUSED-Modus auf 0 gesetzt, was den Aktuator in sichere Position
bringet.

---

## 10. Konfigurations-Architektur

### 10.1 Dreischichtige Config-Hierarchie (ADR-001)

Die Konfiguration folgt einer dreischichtigen Hierarchie:

1. **fw_config.h**: Compile-Time Konstanten (Board-Profile, Pin-Definitionen, Build-Flags)
2. **soft_config.h** (`cfg::`-Namespace): Default-Werte fuer RuntimeConfig
3. **RuntimeConfig**: Mutable RAM-Kopie mit NVS-Persistenz

Ladereihenfolge beim Boot: fw_config → soft_config defaults → NVS → SD-Override

### 10.2 Config Framework (ADR-006)

Das in Schritt 5 implementierte Config Framework bietet:

- **Registry**: `configFrameworkInit()` registriert alle Config-Kategorien
- **Validierung**: `configNtripValidate()`, `configGnssValidate()`, etc. pruefen
  Wertebereiche und Konsistenz
- **Anwendung**: `configNtripApply()` wendet Aenderungen an (kann ntripSetConfig()
  oder Hardware-Reinit triggern)
- **Persistierung**: `configFrameworkSaveAll()` speichert alle Kategorien atomar in NVS
- **Anzeige**: `configNtripShow()` gibt Werte aus (Passwoerter maskiert)
- **Serial Menu**: `configMenuShow()` zeigt kategorisierte Uebersicht

Aktuell implementierte Kategorien: NTRIP, GNSS, Network, System. Fehlende Kategorien:
PID und Actuator (fuer Schritt 5 geplant, teilweise ueber direkte CLI-Kommandos abgedeckt).

### 10.3 Atomic-Persist-Regel

Nur vollstaendige, validierte Konfigurationen werden persistiert:
1. Benutzer aendert Werte im RAM (via CLI oder Menu)
2. `validate()` prueft alle Felder der Kategorie
3. Bei Erfolg: `save()` schreibt atomar in NVS
4. Bei Fehler: Aenderungen verworfen, Fehlermeldung angezeigt

Partielle oder inkonsistente Konfigurationen werden nie persistiert, was Korruption
der NVS-Daten verhindert.

---

## 11. Empfohlene Maßnahmen

| Priorität | Maßnahme | Schritt | ADR | Bemerkung |
|-----------|----------|---------|-----|-----------|
| 1 | Duplicate Code in main.cpp entfernen | 4 | — | bootMaint*/runBoot* Deduplizierung |
| 2 | Betriebsmodus Runtime-Services | 4 | ADR-005 | WiFi/BT Start/Stop bei Moduswechsel |
| 3 | Basis-Konfig-Framework vervollständigen | 5 | ADR-006 | PID + Actuator Kategorien ergänzen |
| 4 | PGN-251/252 Config-Integration | 5 | ADR-PGN-001 | AgIO-Config → Config Framework |
| 5 | Logging Tag-Registry + Filter | 5 | ADR-LOG-002 | Thread-sicherer Runtime-Filter |
| 6 | Feature-Config & Diagnose | 6 | ADR-NTRIP-002 | UM980 Dual-Receiver, erweiterte Diagnose |
| 7 | HAL-Split (hal_impl.cpp) | 7 | ADR-HAL-002 | 2099 Zeilen → 5 Domänenmodule |
| 8 | Module Interface Standardisierung | 7 | ADR-MODULE-001 | Standardisierter Mod-Vertrag |
| 9 | CLI-Modularisierung | 7 | — | 861 Zeilen → thematische Dateien |
| 10 | TASK-045 WDT Hardware-Validierung | — | — | ESP32 Classic ETH DMA unter Last |

---

## 12. Anhang

### 12.1 Dateigrößen-Tabelle (Top 15)

| Datei | Zeilen | Anteil am src/ | Schicht |
|-------|--------|----------------|---------|
| hal_impl.cpp | 2099 | 15.8% | HAL |
| main.cpp | 1186 | 8.9% | Main |
| cli.cpp | 861 | 6.5% | Logic |
| pgn_codec.cpp | 519 | 3.9% | Logic |
| sd_logger_esp32.cpp | 582 | 4.4% | Logic |
| net.cpp | 597 | 4.5% | Logic |
| modules.cpp | 578 | 4.4% | Logic |
| ntrip.cpp | 548 | 4.1% | Logic |
| hal_bno085.cpp | 450 | 3.4% | HAL |
| sd_ota_esp32.cpp | 465 | 3.5% | HAL |
| hw_status.cpp | 300 | 2.3% | Logic |
| control.cpp | 385 | 2.9% | Logic |
| um980_uart_setup.cpp | 173 | 1.3% | Logic |
| pgn_types.h | 368 | — | Logic (Header) |
| modules.h | 185 | — | Logic (Header) |

### 12.2 Modul-Abhängigkeitsgraph

```
main.cpp (Orchestrator)
├── hal_impl.cpp (HAL Init, SPI, ETH, ADC, GPIO, TCP, Pin-Claims)
├── control.cpp → imu.cpp → hal_impl.cpp (IMU SPI)
│               → was.cpp → hal_impl.cpp (ADS1118 SPI)
│               → actuator.cpp → hal_impl.cpp (Actuator SPI)
├── net.cpp → pgn_codec.cpp → hal_impl.cpp (ETH UDP)
│         → hal_impl.cpp (RTCM UART)
│         → op_mode.cpp
├── ntrip.cpp → hal_impl.cpp (TCP Client)
├── cli.cpp → alle Module (thin Dispatcher)
├── modules.cpp → module_interface.h (ModState, ModStateOps)
│             → features.h, fw_config.h
├── sd_logger_esp32.cpp → sd_logger.cpp → hal_impl.cpp (SD Card)
├── hw_status.cpp → StateLock, modules.cpp
├── config_framework.cpp → config_*.cpp → RuntimeConfig, NVS
├── op_mode.cpp → (lock-free atomic)
├── diagnostics.cpp → modules.cpp, hal_impl.cpp
└── setup_wizard.cpp → cli.cpp, nvs_config.cpp
```

### 12.3 Build-Profile-Übersicht

| Profile | Features | MCU | Bemerkung |
|---------|----------|-----|-----------|
| profile_full_steer_ntrip | ALLE (9 Features) | ESP32-S3 | Standard-Produktivprofil |
| T-ETH-Lite-ESP32 | ETH only | ESP32 Classic | Minimalprofil, kein Autosteer |
| T-ETH-Lite-ESP32S3 | ETH only | ESP32-S3 | S3-Minimalprofil |
| native | — | PC (Unit Tests) | Test-Environment |

Das `profile_full_steer_ntrip` ist das Standardprofil und aktiviert alle 9 Features:
`-DFEAT_ETH -DFEAT_IMU -DFEAT_ADS -DFEAT_ACT -DFEAT_SAFETY -DFEAT_GNSS -DFEAT_NTRIP -DFEAT_SD -DFEAT_LOGSW`.
