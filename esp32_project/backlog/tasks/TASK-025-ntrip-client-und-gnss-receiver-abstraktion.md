TASK-025 NTRIP-Client und GNSS-Empfänger-Abstraktion
ID: TASK-025
Titel: NTRIP-Client für Single-Base-Caster und konfigurierbare GNSS-Empfänger-Abstraktion implementieren
Status: done
Priorität: high
Komponenten: src/logic/ntrip.h, src/logic/ntrip.cpp, src/hal/hal.h, src/hal_esp32/hal_impl.cpp, src/logic/global_state.h, src/logic/global_state.cpp, src/logic/dependency_policy.h, src/logic/hw_status.h, include/features.h, src/main.cpp
Dependencies: keine (independent)
delivery_mode: firmware_only
task_category: feature_expansion
Owner: firmware-team
Pflicht-Onboarding für KI-Entwickler-Agenten
⚠️ Verbindlich: Bevor die erste Codezeile geschrieben wird, MUSS der KI-Entwickler folgende Dateien gelesen haben:

README.md
docs/process/PLAN_AGENT.md
docs/process/QUICKSTART_WORKFLOW.md
docs/process/GNSS_BUILDUP.md
Darüber hinaus sind zu lesen (Code-Kontext):

src/logic/features.h — Capability-System verstehen
src/logic/net.cpp / src/logic/net.h — RTCM-Forwarding-Pfad verstehen (UDP-Pfad bleibt bestehen!)
src/logic/global_state.h — Global-State-Muster (g_nav) verstehen
src/logic/dependency_policy.h — Freshness/Validity-Muster verstehen
src/hal/hal.h — HAL-API-Muster (C-Linkage) verstehen
include/hardware_pins.h — Pin-Definitions-Muster verstehen
Fehlende Onboarding-Referenzen gelten als Prozessabweichung gemäß QUICKSTART_WORKFLOW §2e.

classification: independent
exclusive_before: []
parallelizable_after: []
Kontext/Problem
Die Firmware benötigt die Fähigkeit, RTCM-Korrekturdaten direkt von einem NTRIP-Caster (Single-Base, kein VRS) zu beziehen und an lokal angeschlossene GNSS-Empfänger weiterzuleiten. Parallel dazu soll eine generische, konfigurierbare Empfänger-Abstraktion eingeführt werden, die eine beliebige Anzahl (Compile-Time, max. 3) von GNSS-Empfängern unterstützt — sowohl lokal (UART) als auch remote (UDP/PGN).

Bisher existiert ein funktionierender RTCM-UDP-Pfad (AgIO → UDP Port 2233 → Ring-Buffer → GNSS-UART), der nicht verändert wird. NTRIP ist ein paralleler RTCM-Quellen-Pfad, der zusätzlich zum UDP-Pfad existiert.

Die gesamte bestehende GPS-Funktionalität im Code (NMEA-Parsing, Position-Extraktion für PGN 214 etc.) ist Scaffold und kann ignoriert/discarded werden. Die UART-Implementierungen für RTCM-Forwarding sind davon nicht betroffen.

Scope (in)
A) GNSS-Empfänger-Abstraktion
Compile-Time konfigurierbare Empfängerliste (GNSS_RX_MAX = 3).
Jeder Empfänger hat: Transport-Typ (UART / UDP), Verbindungsparameter (UART-Instanz + Pins oder IP + Port), RTCM-Quelle (LOCAL = bekommt RTCM vom lokalen NTRIP-Client, OWN = besorgt sich RTCM selbst, z.B. Remote-Empfänger).
Indizierte HAL-API: hal_gnss_uart_*(uint8_t inst, ...) wobei inst 0..GNSS_RX_MAX-1 entspricht.
Empfängertypen: UM980 und UM982 (NMEA identisch; UM982 Heading wird in diesem Task nicht berücksichtigt).
UART0 kann nach Boot-Vorgang optional als GNSS-UART verwendet werden (Notfall/Fallback).
B) NTRIP-Client
TCP-Verbindung zu einem Single-Base NTRIP-Caster über bestehendes Ethernet (W5500).
HTTP-Request mit Base64 Basic Auth (NTRIP Protocol Spec, Rev 2.0).
RTCM-Datenstrom vom Caster lesen und in Global State puffern.
RTCM-Forwarding an alle Empfänger mit rtcm_source = LOCAL.
GGA-Rücksendung an den Caster ist nicht erforderlich (Single Base, kein VRS).
Automatischer Reconnect bei Verbindungsabbruch.
Verbindungsstatus und Statistiken (Bytes empfangen, Bytes forwarded, Verbindungsfehler) in Global State.
C) Integration
FEAT_NTRIP Capability-Flag mit Abhängigkeit FEAT_GNSS && FEAT_COMM.
NTRIP-Funktionen in den bestehenden commTask-Loop (Core 0, 100Hz) integriert.
NTRIP-spezifische Timeouts in dependency_policy.h.
HW_GNSS Subsystem in hw_status.h (ID = 5).
Architektur-Muster: Input → Global State → Processing → Output (prozedurale Funktionen, KEINE Klassen).
Nicht-Scope (out)
GGA-Parsing / GgaParser (nicht benötigt bei Single-Base-Caster).
"Bester Empfänger"-Auswahllogik (nicht benötigt ohne GGA-Rücksendung).
Remote-Empfänger-Datenpfad (PGN-Empfang, Position-Auswertung, PGN 214 Befüllung) — separater Task.
Bestehender UDP-RTCM-Pfad (TASK-015) — wird nicht verändert.
UM982 Heading-Ausgabe.
NMEA-Parsing / Position-Extraktion aus Empfängern.
Designentscheidungen
#       Entscheidung    Begründung
D1      Kein GGA an NTRIP-Caster        Caster ist Single-Base, keine positionsabhängigen Korrekturdaten
D2      Max. 3 Empfänger (Compile-Time) ESP32-S3: UART1, UART2 + UART0 nach Boot; GNSS_RX_MAX = 3
D3      Empfänger-Abstraktion transport-agnostisch      Lokal (UART) und Remote (UDP/PGN) unterscheiden sich nur in Konfiguration
D4      Prozedurale Funktionen, keine Klassen   Konform mit bestehendem Architekturmuster (net.cpp, control.cpp)
D5      NTRIP-TCP über bestehendes Ethernet     W5500 Ethernet-Stack wird mitgenutzt, kein neuer Netzwerk-Stack
D6      RTCM-Forwarding nur an rtcm_source=LOCAL Empfänger      Remote-Empfänger besorgen RTCM selbst
D7      Indizierte UART-API     Ersatz für aktuelle Single-UART-API, Rückwärtskompatibilität via Wrapper
Architektur-Übersicht
Datenfluss
┌─────────────────────────────────────────────┐
│ Global State │
│ ┌─────────┐ ┌─────────┐ ┌────────────┐ │
│ │g_ntrip │ │g_gnss_rx│ │g_nav │ │
│ │.rtcm_buf│ │[0..NMAX]│ │(bestehend)│ │
│ │.status │ │.config │ │ │ │
│ │.stats │ │.status │ │ │ │
│ └────┬─────┘ └────┬────┘ └────────────┘ │
└───────┼─────────────┼───────────────────────┘
│ │
┌─────────────┘ └──────────────┐
│ Input │ Output
▼ ▼
┌────────────────┐ ┌─────────────────────┐
│ ntripReadRtcm()│ │ ntripForwardRtcm() │
│ TCP → rtcm_buf │ │ rtcm_buf → UART[0] │
└────────────────┘ │ rtcm_buf → UART[1] │
│ (alle LOCAL-RX) │
└─────────────────────┘
text


### Empfänger-Konfiguration (Compile-Time)

```cpp
// Beispiel-Konfiguration (Board-spezifisch)
constexpr uint8_t GNSS_RX_MAX = 2;

struct GnssRxConfig {
    enum Transport { UART, UDP } transport;
    // UART
    uint8_t uart_inst;        // 0=UART0, 1=UART1, 2=UART2
    int8_t tx_pin;
    int8_t rx_pin;
    uint32_t baud;
    // UDP (remote)
    uint8_t remote_ip[4];
    uint16_t remote_port;
    // RTCM-Quelle
    enum RtcmSource { LOCAL, OWN } rtcm_source;
};

// Board-spezifische Konfiguration
constexpr GnssRxConfig GNSS_RX_CONFIGS[GNSS_RX_MAX] = {
    { GnssRxConfig::UART, 1, 48, 45, 115200, {}, 0, GnssRxConfig::LOCAL },
    { GnssRxConfig::UART, 2,  2,  1, 115200, {}, 0, GnssRxConfig::LOCAL },
};
NTRIP-State-Maschine
text

IDLE → CONNECTING → AUTHENTICATING → CONNECTED
  ↑       │              │               │
  └───────┴──── ERROR ───┘               │
               │                          │
               └───── DISCONNECTED ───────┘
Datei-Footprint-Analyse
files_read
include/hardware_pins.h — bestehende Pin-Definitionen, Board-Profil-Muster
include/board_profile/*.h — board-spezifische Pin-Belegungen
src/logic/features.h — Capability-System, bestehende Feature-Flags
src/logic/global_state.h — Global-State-Muster (g_nav)
src/logic/global_state.cpp — Instanziierungsmuster
src/logic/dependency_policy.h — Freshness/Validity-Muster
src/logic/hw_status.h — Subsystem-Monitoring-Muster
src/logic/net.h / src/logic/net.cpp — RTCM-Forwarding (Verständnis, KEINE Änderung)
src/logic/control.h / src/logic/control.cpp — Prozedurales Funktions-Muster (Referenz)
src/hal/hal.h — HAL-API-Muster, bestehende GNSS-RTCM-Funktionen
src/hal_esp32/hal_impl.cpp — ESP32 HAL-Implementierung, UART-Setup, Ethernet
src/hal_esp32/hal_impl.h — ESP32-spezifische Init-Deklarationen
src/main.cpp — commTask-Loop, FreeRTOS-Tasks, GNSS-Mirror
platformio.ini — Build-Flags, Environments
README.md
docs/process/PLAN_AGENT.md
docs/process/QUICKSTART_WORKFLOW.md
docs/process/GNSS_BUILDUP.md
files_write
src/logic/ntrip.h — NEU: NTRIP-Funktionsdeklarationen, NtripConfig, NtripState
src/logic/ntrip.cpp — NEU: NTRIP-Implementierung (Connect, Auth, RTCM-Rx, Forward)
src/hal/hal.h — ÄNDERN: Indizierte hal_gnss_uart_*(uint8_t inst, ...) API + hal_tcp_* TCP-Client-API
src/hal_esp32/hal_impl.cpp — ÄNDERN: Indizierte UART-Implementierung, TCP-Client (EthernetClient)
src/hal_esp32/hal_impl.h — ÄNDERN: Zusätzliche Init-Deklarationen für NTRIP/GNSS-RX
src/logic/global_state.h — ÄNDERN: NtripState-Struct, GnssRxState-Struct (pro Empfänger)
src/logic/global_state.cpp — ÄNDERN: g_ntrip, g_gnss_rx Instanz-Definitionen
src/logic/dependency_policy.h — ÄNDERN: NTRIP-spezifische Timeouts
src/logic/hw_status.h — ÄNDERN: HW_GNSS Subsystem
include/features.h — ÄNDERN: FEAT_NTRIP Capability-Flag + feat::ntrip()
src/main.cpp — ÄNDERN: NTRIP-Funktionen in commTask-Loop integrieren
public_surface
src/hal/hal.h — Neue öffentliche HAL-API (indizierte GNSS-UART + TCP-Client)
src/logic/ntrip.h — Öffentliche NTRIP-Schnittstelle
src/logic/global_state.h — Neue Global-State-Structs (NtripState, GnssRxState)
include/features.h — Neues Feature-Flag FEAT_NTRIP
src/logic/dependency_policy.h — Neue Timeout-Konstanten
src/logic/hw_status.h — Neuer Subsystem-Enum-Wert
merge_risk_files
src/hal/hal.h — Zentrale HAL-Schnittstelle, wird von vielen Tasks referenziert
src/hal_esp32/hal_impl.cpp — Große Implementierungsdatei, mehrere Init-Pfade
src/main.cpp — Entry-Point, Task-Loop
include/features.h — Compile-Time-Gating, zentraler Header
src/logic/global_state.h — Globaler State, viele Abhängigkeiten
backlog/index.yaml — Backlog-Index
risk_notes
hal.h ist merge-risk: Gleichzeitige Änderungen durch andere Tasks (TASK-019F, TASK-019G) möglich. Vor Änderung claimen und exklusiv locken.
hal_impl.cpp ist groß (1745 Zeilen): Änderungen müssen isoliert und rückwärtskompatibel sein. Bestehende Single-UART-API muss als Wrapper erhalten bleiben.
UART0 als GNSS-Fallback: Serial-Console wird nach Boot freigegeben. Timing und Flush-Verhalten müssen beachtet werden.
NTRIP-TCP und UDP-RTCM parallel: Beide Pfade nutzen das gleiche Ethernet. Bandbreite und Socket-Limits beachten (W5500 hat max. 8 Sockets).
Remote-Empfänger in Zukunft: Die Empfänger-Abstraktion muss erweiterbar sein, aber die UDP-Datenpfad-Implementierung ist out-of-scope für diesen Task.
Akzeptanzkriterien (AC)
AC-1: Indizierte GNSS-UART HAL
hal_gnss_uart_begin(uint8_t inst, uint32_t baud, int8_t rx_pin, int8_t tx_pin) initialisiert den UART für Empfänger inst.
hal_gnss_uart_write(uint8_t inst, const uint8_t* data, size_t len) schreibt RTCM-Daten an Empfänger inst.
hal_gnss_uart_is_ready(uint8_t inst) prüft, ob Empfänger inst initialisiert ist.
Bestehende hal_gnss_rtcm_*-API bleibt als Wrapper auf inst=0 funktionsfähig (Rückwärtskompatibilität).
AC-2: TCP-Client HAL
hal_tcp_connect(const char* host, uint16_t port) öffnet TCP-Verbindung über Ethernet.
hal_tcp_write(const uint8_t* data, size_t len) sendet Daten.
hal_tcp_read(uint8_t* buf, size_t max_len) liest verfügbare Daten (non-blocking).
hal_tcp_connected() prüft Verbindungsstatus.
hal_tcp_disconnect() schließt Verbindung.
AC-3: NTRIP-Client
Verbindet sich zu konfigurierbarem NTRIP-Caster (Host, Port, Mountpoint, User, Pass).
Sendet NTRIP-Request mit Base64 Basic Auth.
Liest RTCM-Datenstrom und puffert in g_ntrip.rtcm_buf.
State-Maschine: IDLE → CONNECTING → AUTHENTICATING → CONNECTED → (ERROR → Reconnect).
Automatischer Reconnect mit konfigurierbarer Verzögerung (Default: 5s).
Verbindungsstatistiken: Bytes empfangen, Bytes forwarded, Connect-Failures, letztes RTCM-Timestamp.
AC-4: RTCM-Forwarding
RTCM-Daten werden an alle Empfänger mit rtcm_source = LOCAL weitergeleitet.
Forwarding erfolgt pro Empfänger über die indizierte hal_gnss_uart_write(inst, ...) API.
Statistiken pro Empfänger: Bytes forwarded, Drop-Count.
AC-5: Compile-Time Konfiguration
GNSS_RX_MAX definiert maximale Empfängeranzahl (Default: 2).
Empfängerkonfiguration als constexpr GnssRxConfig[] Array (Board-spezifisch).
FEAT_NTRIP Feature-Flag schaltet NTRIP-Funktionalität compile-time ein/aus.
Ohne FEAT_NTRIP wird kein NTRIP-Code kompiliert (null overhead).
AC-6: Integration
NTRIP-Funktionen sind in den commTask-Loop integriert:
Input-Phase: ntripReadRtcm()
Processing-Phase: ntripTick() (State-Machine)
Output-Phase: ntripForwardRtcm()
HW_GNSS (ID=5) Subsystem-Monitoring: Verbindungsstatus wird über hwStatusSetFlag() gemeldet.
NTRIP-Timeouts in dependency_policy.h: NTRIP_RTCM_FRESHNESS_TIMEOUT_MS, NTRIP_RECONNECT_DELAY_MS.
AC-7: Rückwärtskompatibilität
Bestehender UDP-RTCM-Pfad (TASK-015, net.cpp) funktioniert unverändert.
Bestehende hal_gnss_rtcm_*-API kompiliert und funktioniert weiter.
gnss_buildup Environment baut fehlerfrei (mit und ohne NTRIP).
AC-8: Entwickler-Report
Report unter reports/TASK-025/<dev-name>.md gemäß templates/dev-report.md.
Umsetzungs-Inkremente
INC-1: HAL-Erweiterung (merge-risk)
Indizierte hal_gnss_uart_*(uint8_t inst, ...) API in hal.h deklarieren.
Implementierung in hal_impl.cpp: HardwareSerial* _gnssUart[GNSS_RX_MAX], Init pro Instanz, Pin-Claiming.
hal_tcp_* API in hal.h deklarieren (W5500 EthernetClient).
TCP-Implementierung in hal_impl.cpp.
Rückwärtskompatibilitäts-Wrapper: alte hal_gnss_rtcm_* → Delegation an inst=0.
Verifikation: Build-Check pio run -e gnss_buildup, keine Regression in bestehendem UDP-Pfad.
INC-2: Global State, Features, Policy, HW-Status
NtripConfig und NtripState Structs in global_state.h.
GnssRxConfig und GnssRxState Structs (pro Empfänger) in global_state.h.
g_ntrip und g_gnss_rx Instanzen in global_state.cpp.
FEAT_NTRIP in features.h mit Abhängigkeiten und feat::ntrip().
HW_GNSS = 5 in hw_status.h.
NTRIP-Timeouts in dependency_policy.h.
Verifikation: Build-Check, Header-Kompilierbarkeit.
INC-3: NTRIP-Client-Logik
src/logic/ntrip.h: Funktionsdeklarationen (prozedural).
src/logic/ntrip.cpp: State-Maschine, TCP-Connect/Auth, RTCM-Stream lesen, Reconnect-Logik.
Verifikation: Build-Check, Code-Review gegen State-Maschinen-Spec.
INC-4: Integration und Feature-Gating
NTRIP-Funktionen in main.cpp commTask-Loop einbinden.
Feature-Gating: #if FEAT_ENABLED(FEAT_NTRIP) um alle NTRIP-Aufrufe.
Empfängerkonfiguration (Board-spezifisch) in Board-Profil-Header.
Verifikation: Build-Check alle Environments, Entwickler-Report.
Verifikation/Test
Build-Check: pio run für alle relevanten Environments (gnss_buildup, esp32dev, esp32-s3).
Statischer Review: HAL-API gegen Task-Spec (AC-1, AC-2).
Feature-Gating-Test: Build mit und ohne FEAT_NTRIP — kein NTRIP-Code im Binary ohne Flag.
Rückwärtskompatibilität: Bestehende hal_gnss_rtcm_*-API-Aufrufe kompilieren und funktionieren.
Prozess-Compliance: Entwickler-Report vorhanden, Branch korrekt, Onboarding gelesen.
Links
backlog/epics/EPIC-004-feature-expansion.md
backlog/tasks/TASK-015-udp-rtcm-receiver-und-buffering.md (paralleler RTCM-Pfad, wird nicht verändert)
backlog/tasks/TASK-014-hal-gnss-rtcm-uart-forwarding.md (bestehende UART-Forwarding-Implementierung)
docs/process/GNSS_BUILDUP.md (GNSS-Buildup-Environment, Smoke-Tests)
docs/process/PLAN_AGENT.md (Merge-Risk-Regeln)
docs/process/QUICKSTART_WORKFLOW.md (Onboarding-Pflicht)