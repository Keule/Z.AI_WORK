# TASK-025 Entwickler-Report

## Task
NTRIP-Client fuer Single-Base-Caster und konfigurierbare GNSS-Empfaenger-Abstraktion implementieren

## Status
done

## Implementierung

### Umfang
- NTRIP-Client (TCP-Connect, HTTP/1.0 Auth, RTCM Stream)
- Multi-Empfaenger GNSS-Abstraktion (GnssRxConfig/GnssRxState)
- RTCM Ring-Buffer fuer TCP->UART Forwarding
- NTRIP State Machine (IDLE -> CONNECTING -> AUTHENTICATING -> CONNECTED -> ERROR/DISCONNECTED)
- Feature-Gating via FEAT_NTRIP
- HW_GNSS Subsystem (ID=5) in hw_status.h
- NTRIP-Status in hwStatusUpdate() beruecksichtigt (TASK-030)

### Dateien
- `src/logic/ntrip.h` (neu) — NTRIP-Protokoll API-Deklarationen
- `src/logic/ntrip.cpp` (neu) — NTRIP-Protokollimplementierung
- `src/hal/hal.h` — Indizierte hal_gnss_uart_*(uint8_t inst, ...) + hal_tcp_* API
- `src/hal_esp32/hal_impl.cpp` — TCP-Client (WiFiClient), indexed GNSS UART
- `src/logic/global_state.h` — NtripConfig, NtripState, GnssRxConfig/State
- `src/logic/global_state.cpp` — g_ntrip_config, g_ntrip Instanzen
- `src/logic/features.h` — FEAT_NTRIP Capability
- `src/logic/hw_status.h` — HW_GNSS = 5, hwStatusUpdate() +ntrip_active Parameter (TASK-030)
- `src/logic/hw_status.cpp` — GNSS/NTRIP Flag-Clearing in hwStatusUpdate() (TASK-030)
- `src/logic/dependency_policy.h` — NTRIP_RTCM_FRESHNESS_TIMEOUT_MS, NTRIP_RECONNECT_DELAY_MS
- `src/main.cpp` — NTRIP Init/Config, commTask Integration, MOD_NTRIP an hwStatusUpdate (TASK-030)
- `include/soft_config.h` — cfg:: NTRIP-Defaults (TASK-028)
- `src/logic/runtime_config.h` / `runtime_config.cpp` — RuntimeConfig NTRIP-Felder (TASK-028)

### Build-Fixes (Vorgaenger-Session)
1. `std::snprintf` -> `snprintf` (PlatformIO ESP32 toolchain)
2. `LOG_LEVEL_NTRIP` hinzugefuegt zu log_config.h
3. `*out_status = 401` -> `*out_status = 1` (uint8_t Overflow)

## Test
- Compile-Check: `pio run -e gnss_buildup` und `pio run -e gnss_bringup_ntrip` erfolgreich
- Kein Hardware-Test durchgefuehrt (Firmware-only)

## Architektur-Entscheidungen
- NTRIP laeuft in commTask (spaeter in maintTask via TASK-029)
- RTCM Ring-Buffer: 2 KB statisch (spaeter PSRAM via TASK-029)
- GNSS-Empfaenger indexed: inst=0 (legacy UART), inst=1..N (neu)
- WiFiClient fuer TCP (kompatibel mit ESP-IDF ETH)
- HW_GNSS Status wird von ntripTick() gesetzt (OK/WARNING), von hwStatusUpdate() nur
  geclear wenn NTRIP-Modul inaktiv (TASK-030)
- hwStatusUpdate() erhielt neuen Parameter `ntrip_active` mit Default=true fuer
  Rueckwaertskompatibilitaet (TASK-030)

## Offene Punkte
- NTRIP-Konfiguration war hardcoded -> jetzt via soft_config.h (TASK-028)
- NTRIP-Connect war blocking in commTask -> jetzt in maintTask (TASK-029)
- NTRIP fehlte im Modul-System -> jetzt integriert (TASK-027)
- NTRIP-Status war nicht in hwStatusUpdate() beruecksichtigt -> jetzt integriert (TASK-030)
