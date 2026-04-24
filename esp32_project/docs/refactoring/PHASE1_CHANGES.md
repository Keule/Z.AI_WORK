# Phase 1: Feature-Gates — Aenderungsprotokoll

## Datum: 2026-04-22
## Agent: GPT-5.3-Codex

## Aenderungen

### P1: HAL Init feature-blind
- [x] `hal_esp32_init_all()`: Guard um `hal_sensor_spi_init()`
- [x] `hal_esp32_init_all()`: Guard um `hal_imu_begin()`
- [x] `hal_esp32_init_all()`: Guard um `hal_steer_angle_begin()`
- [x] `hal_esp32_init_all()`: Guard um `hal_actuator_begin()`
- [ ] `hal_esp32_init_all()`: Guard um `hal_gnss_rtcm_begin()`

### P1: controlStep Feature-Gates
- [x] Guard um `imuUpdate()` Aufruf
- [x] Guard um `steerAngleReadDeg()` Aufruf
- [x] Guard um PID+Aktor Block
- [x] Safety + Watchdog NICHT geguardet (verifiziert)

### P7: RTCM Feature-Gate
- [x] Guard um RTCM-Funktionen in `net.cpp`
- [x] RTCM-Aufrufstelle geprueft: kein direkter `main.cpp`-Call, Guard in `netPollReceive()` gesetzt

### P2: Orphaned Init entfernt
- [x] `imuInit()` entfernt aus `imu.h` / `imu.cpp`
- [x] `steerAngleInit()` entfernt aus `steer_angle.h` / `steer_angle.cpp`
- [x] `actuatorInit()` entfernt aus `actuator.h` / `actuator.cpp`
- [x] Keine verbleibenden Referenzen (grep verifiziert)

## Offene Punkte / Fragen
- `hal_gnss_rtcm_begin()` wird aktuell nur im GNSS-Buildup-Pfad verwendet, nicht in `hal_esp32_init_all()`. Daher kein zusaetzlicher Guard in `hal_esp32_init_all()` gesetzt.
- In `main.cpp` existiert keine direkte Aufrufstelle von `netPollRtcmReceiveAndForward()`; der RTCM-Pfad laeuft ueber `netPollReceive()`. Dort wurde der Guard gesetzt.

## Nicht geaendert (Absicht)
- `hal_esp32_init_imu_bringup()` — alternativer Boot-Pfad
- `hal_esp32_init_gnss_buildup()` — alternativer Boot-Pfad
- PGN-Protokoll-Codec — keine Aenderungen
- Safety-Logik — unveraendert
- State-Struktur — unveraendert (Phase 3)
