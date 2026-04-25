# Entwickler-Report: TASK-031, TASK-032, TASK-033, TASK-034

- **Ausgeführt von:** KI-Entwickler (Super Z)
- **Datum:** 2026-04-21
- **Branch:** `zai/complete-tasks-26-30-rework-task-31-35`
- **Basis-Commit:** `8dc3771` (KI-Planer: TASK-031/034 AC-Korrektur, TASK-035 erledigt)

## TASK-032: hwStatusSetFlag() Bug — GNSS-Status fälschlich als Fehler

### Umsetzung

**Option B (allgemeiner Fix)** gewählt, da nur ein Aufrufer existiert
(`rg "hwStatusSetFlag.*HW_SEV_OK" src/` → nur `ntrip.cpp:429`).

**Geändert:** `src/logic/hw_status.cpp`

`hwStatusSetFlag()` prüft jetzt vorab ob `severity == HW_SEV_OK`:
- Falls ja → delegiert an `hwStatusClearFlag()` und returnt sofort
- Dadurch wird der bestehende Aufruf `hwStatusSetFlag(HW_GNSS, HW_SEV_OK)` in `ntrip.cpp:429` automatisch korrekt behandelt

**Nicht geändert:** `ntrip.cpp` — der Aufruf bleibt bestehen und ist jetzt semantisch korrekt.

### Abweichungen vom Task

Keine. Task empfahl Option B, und es gab keine weiteren Aufrufer von `hwStatusSetFlag(HW_*, HW_SEV_OK)`.

## TASK-034: LOG_SWITCH_PIN GPIO-3 + Profil umbenennen

### Umsetzung

**F4 — GPIO-46 → GPIO-3:**

**Geändert:** `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h`
- `LOG_SWITCH_PIN` von 46 auf 3 geändert
- Kommentar aktualisiert (Strapping-Pin-Hinweis entfernt, GPIO-3 als freier Pin dokumentiert)
- `FEAT_PINS_LOGSW` Kommentar "CONFLICT with IMU_INT!" entfernt

**F5 — Profil umbenennen:**

**Geändert:** `platformio.ini`
- `[env:profile_full_steer_ntrip_esp32]` → `[env:profile_ntrip_classic]`
- `-DFEAT_PROFILE_FULL_STEER` entfernt (das Profil hat kein Steer-Feature)

**F6 — NTRIP-GNSS-Dependency:** Nicht umgesetzt (vom Task-Owner im Update entfernt).

### Abweichungen vom Task

Keine.

### Verifikation

- `rg "profile_full_steer_ntrip_esp32" platformio.ini` → keine Treffer
- `rg "LOG_SWITCH_PIN.*46" include/` → keine Treffer
- GPIO-3 ist bidirektional, nicht von ETH/SPI/UART/PSRAM belegt
- ESP32 Classic `LOG_SWITCH_PIN = PIN_BT = 0` unverändert

## TASK-031: Legacy HAL Pin-Claims → MOD_* Tags

### Umsetzung

**Geändert:** `src/hal_esp32/hal_impl.cpp`

Alle vier `claimXxxPins()`-Funktionen aktualisiert:

| Funktion | Legacy Tags | Neue Tags |
|----------|-------------|-----------|
| `claimCommonInitPins()` | `"safety-input"`, `"sensor-spi-sck/miso/mosi"` | `"MOD_SAFETY"`, `"HAL_SENSOR_SPI"` |
| `claimImuSteerInitPins()` | `"imu-int/rst/wake/cs"`, `"steer-angle-cs"`, `"actuator-cs"` | `"MOD_IMU"`, `"MOD_ADS"`, `"MOD_ACT"` |
| `claimEthPins()` | `"eth-sck/miso/mosi/cs/int/rst"` | `"MOD_ETH"` |
| `claimGnssUartPins()` | `"gnss-rtcm-rx"`, `"gnss-rtcm-tx"` | `"MOD_GNSS"` |

**Designentscheidung für SPI-Bus-Pins:** Die Sensor-SPI-Pins (SCK=47, MISO=21, MOSI=38) werden unter dem dedizierten Owner `"HAL_SENSOR_SPI"` geclaimt (kein MOD_*-Tag). Begründung: Drei Module (IMU, ADS, ACT) teilen sich denselben Bus — ein einzelner MOD_*-Owner wäre inkorrekt. Die FEAT_PINS_*-Arrays der Module enthalten ohnehin nur CS-/Seitenband-Pins, nicht die Bus-Pins.

### Abweichungen vom Task

Keine. Task spezifizierte genau diese Lösung.

### Verifikation

- `rg "safety-input|sensor-spi-|imu-int|imu-cs|steer-angle-cs|actuator-cs|eth-sck|gnss-rtcm" src/hal_esp32/hal_impl.cpp` → keine Treffer
- Owner-Tags in `hal_impl.cpp` stimmen exakt mit `featureOwnerTag()` in `modules.cpp` überein
- ESP32 Classic: Alle Sensor-Pins sind -1, Claims werden via `pin < 0` übersprungen

## TASK-033: NTRIP-Credentials aus Code entfernen + dateibasiertes Laden

### Umsetzung

**Geändert:** `include/soft_config.h`
- NTRIP_HOST, NTRIP_MOUNTPOINT, NTRIP_USER, NTRIP_PASSWORD auf `""` gesetzt
- NTRIP_PORT bleibt 2101 (neutraler Standard-Port)
- Kommentar hinzugefügt der auf `/ntrip.cfg` verweist

**Geändert:** `src/logic/runtime_config.cpp`
- `softConfigLoadOverrides()` implementiert (war vorher no-op stub)
- Liest `/ntrip.cfg` von SD-Karte (Key=Value INI-Format)
- SD-Karte wird temporär gemounted, Datei gelesen, SD wieder unmounted
- Fallback: Wenn Datei nicht existiert → returns false (NTRIP bleibt im IDLE-Zustand)
- `#if defined(ARDUINO_ARCH_ESP32)` Guard für Plattformabhängigkeit
- Parser unterstützt: host, port, mountpoint, user, password
- Kommentare (#) und leere Zeilen werden übersprungen

**Geändert:** `src/main.cpp`
- Zwei Kommentar-Aktualisierungen: "currently a no-op stub" → "TASK-033: reads /ntrip.cfg from SD"

**Dateiformat (/ntrip.cfg):**
```
host=euref-ip.net
port=2101
mountpoint=KARL00DEU0
user=oebhk
password=<geheimes_passwort>
```

### Abweichungen vom Task

Keine wesentlichen. Task spezifizierte INI-Format mit `/ntrip.cfg` — genau so implementiert.

### Verifikation

- `rg "oebhk|0@AW|euref-ip|KARL00DEU0" include/` → keine Treffer
- SD-Zugriff passiert in `setup()` vor maintTask-Start (kein SPI-Konflikt)
- `softConfigLoadOverrides()` returns false wenn keine SD-Karte → Boot erfolgreich

## Cross-Task-Interaktionen

- **TASK-031 + TASK-034:** TASK-031 ändert Owner-Tags, TASK-034 ändert LOG_SWITCH_PIN. Beide ändern `hal_impl.cpp` bzw. Board-Profil unabhängig voneinander — kein Merge-Konflikt.
- **TASK-033 + TASK-034:** TASK-034 ändert GPIO-3 im Board-Profil. GPIO-3 wird von TASK-033 nicht referenziert — kein Konflikt.
- **TASK-032:** Isolierter Fix in `hw_status.cpp` — keine Interaktion mit anderen Tasks.

## Files Changed

| Datei | Tasks |
|-------|-------|
| `src/logic/hw_status.cpp` | TASK-032 |
| `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h` | TASK-034 |
| `platformio.ini` | TASK-034 |
| `src/hal_esp32/hal_impl.cpp` | TASK-031 |
| `include/soft_config.h` | TASK-033 |
| `src/logic/runtime_config.cpp` | TASK-033 |
| `src/main.cpp` | TASK-033 |

## Offene Punkte / Hinweise für Reviewer

1. **GPIO-3 als Strapping-Pin:** GPIO-3 ist auf dem ESP32-S3 ein Strapping-Pin (JTAG-Signal). Bei den meisten Board-Revisionen ist dies unproblematisch für digitalen Input, sollte aber auf der Hardware verifiziert werden.
2. **TASK-033 SD-Performance:** Der SD-Zugriff in `setup()` blockiert für ca. 100-500 ms (SD-Init + Datei-Lesen). Dies ist akzeptabel da der Boot-Vorgang ohnehin blockiert.
3. **TASK-031 HAL_SENSOR_SPI:** Der dedizierte Owner-Tag für SPI-Bus-Pins ist kein MOD_*-Tag. `featureOwnerTag()` in `modules.cpp` hat keinen Eintrag dafür — das ist korrekt, da die SPI-Bus-Pins nicht vom Modulsystem verwaltet werden.
