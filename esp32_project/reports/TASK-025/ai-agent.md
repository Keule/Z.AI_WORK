Entwickler-Report für Task TASK-025

Entwickler: AI Agent (Super Z)
Task-ID: TASK-025

Checkliste (Pflichtfelder)

- [x] Zusammenfassung ausgefüllt
- [x] Geänderte Dateien vollständig aufgelistet
- [x] Tests / Build dokumentiert (inkl. Ergebnis)
- [x] Offene Fragen / Probleme dokumentiert

Zusammenfassung

TASK-025 implementiert einen NTRIP-Client fuer Single-Base-Caster und eine konfigurierbare GNSS-Empfaenger-Abstraktion. Die Implementierung folgt strikt den 4 Inkrementen aus der Task-Spec:

1. **INC-1 (HAL-Erweiterung)**: Indizierte `hal_gnss_uart_*(uint8_t inst, ...)` API und TCP-Client `hal_tcp_*` API in `hal.h`/`hal_impl.cpp`. Bestehende `hal_gnss_rtcm_*`-API bleibt als Wrapper auf inst=0 voll funktionsfaehig (Rueckwaertskompatibilitaet).

2. **INC-2 (Global State, Features, Policy, HW-Status)**: `NtripConfig`, `NtripState`, `GnssRxConfig`, `GnssRxState` Structs in `global_state.h`. `FEAT_NTRIP` Feature-Flag in `features.h` mit Abhaengigkeit `FEAT_GNSS && FEAT_COMM`. `HW_GNSS = 5` Subsystem in `hw_status.h`. NTRIP-Timeouts in `dependency_policy.h`.

3. **INC-3 (NTRIP-Client-Logik)**: Neue Dateien `ntrip.h` und `ntrip.cpp` mit State-Maschine (IDLE → CONNECTING → AUTHENTICATING → CONNECTED → ERROR → Reconnect), Base64 Basic Auth, RTCM-Ring-Buffer, und Forwarding an LOCAL-Empfaenger.

4. **INC-4 (Integration)**: NTRIP-Funktionen in `commTaskFunc` in `main.cpp` integriert (Input/Processing/Output Phasen), vollstaendig Feature-geguardet (`#if FEAT_ENABLED(FEAT_NTRIP)`).

Geänderte Dateien

Neu erstellt:
- `src/logic/ntrip.h` — NTRIP-Funktionsdeklarationen, prozedural
- `src/logic/ntrip.cpp` — NTRIP-Implementierung (Connect, Auth, RTCM-Rx, Forward, State-Maschine)

Geändert (merge-risk):
- `src/hal/hal.h` — Indizierte hal_gnss_uart_* API + hal_tcp_* API + GNSS_RX_MAX
- `src/hal_esp32/hal_impl.cpp` — Implementierung indizierter UART + TCP Client (WiFiClient)
- `src/logic/global_state.h` — NtripConfig, NtripState, GnssRxConfig, GnssRxState, NTRIP_RTCM_BUF_SIZE
- `src/logic/global_state.cpp` — g_ntrip_config, g_ntrip Instanz-Definitionen
- `src/logic/features.h` — FEAT_NTRIP Capability-Flag + feat::ntrip()
- `src/logic/hw_status.h` — HW_GNSS = 5 Subsystem
- `src/logic/hw_status.cpp` — s_subsys_names Array erweitert
- `src/logic/dependency_policy.h` — NTRIP_RTCM_FRESHNESS_TIMEOUT_MS, NTRIP_RECONNECT_DELAY_MS
- `src/main.cpp` — NTRIP-Funktionen in commTask-Loop, Feature-geguardet
- `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h` — GNSS Receiver Konfigurations-Kommentar

Nicht geändert (explizit):
- `src/logic/net.cpp` — UDP-RTCM-Pfad bleibt unverändert (AC-7)

Tests / Build

Build-Ergebnisse (PlatformIO):

| Environment       | Status  | RAM      | Flash     |
|-------------------|---------|----------|-----------|
| gnss_buildup      | SUCCESS | 15.9%    | 70.5%     |
| gnss_buildup_s3   | SUCCESS | 15.8%    | 22.3%     |

Feature-Gating-Test:
- Ohne `-DFEAT_NTRIP`: NTRIP-Code wird komplett ausgeschlossen (0 Overhead). Build erfolgreich.
- Mit `-DFEAT_NTRIP -DFEAT_GNSS`: NTRIP-Code wird kompiliert (kompilierbar, nicht auf Hardware getestet).

Statischer Review gegen ACs:
- AC-1: Indizierte GNSS-UART HAL implementiert, Legacy-Wrapper vorhanden
- AC-2: TCP-Client HAL (hal_tcp_connect/write/read/available/connected/disconnect) implementiert
- AC-3: NTRIP-Client mit State-Maschine, Base64 Auth, Reconnect implementiert
- AC-4: RTCM-Forwarding an LOCAL-Empfaenger implementiert
- AC-5: GNSS_RX_MAX = 2 (Default), FEAT_NTRIP Feature-Gating
- AC-6: Integration in commTask-Loop (Input/Processing/Output)
- AC-7: Rueckwaertskompatibilitaet gewahrt, net.cpp unverändert
- AC-8: Entwickler-Report erstellt

Offene Fragen / Probleme

1. **Kein Hardware-Test moeglich**: Die Implementierung kann nicht auf echter Hardware getestet werden. Der Mensch muss verifizieren:
   - NTRIP-Verbindung zu einem Caster
   - RTCM-Forwarding an GNSS-Empfaenger
   - Reconnect-Verhalten bei Verbindungsabbruch

2. **Kein Push moeglich**: Der Agent kann keine Git-Pushes durchfuehren. Der Mensch muss aendern und mergen.

3. **UART0 als GNSS-Fallback**: Die Task-Spec erwaehnt UART0 als optionalen Fallback nach Boot. Dies wurde bewusst NICHT implementiert, da die Prioritaet auf dem Hauptpfad (UART1/UART2) lag und UART0 die Serial-Console belegt. Falls benoetigt, kann dies in einem Folge-Task ergaenzt werden.

4. **NTRIP-Konfiguration**: Die NTRIP-Caster-Parameter (Host, Port, Mountpoint, User, Passwort) muessen vor der ersten Verbindung gesetzt werden. Eine Implementierung fuer persistente Konfiguration (z.B. NVS) ist out-of-scope.

5. **WiFiClient vs EthernetClient**: Die TCP-Verbindung nutzt `WiFiClient`, welches mit dem ESP-IDF ETH-Treiber funktioniert (Ethernet-Interface wird automatisch vom lwip-Stack genutzt). Dies wurde gegen den naecheliegenden `EthernetClient` gewaehlt, da `EthernetClient` nicht vom ESP-IDF ETH-Treiber bereitgestellt wird.
