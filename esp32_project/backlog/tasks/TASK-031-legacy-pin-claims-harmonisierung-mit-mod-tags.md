# Task: Legacy HAL Pin-Claims mit MOD_*-Tags harmonisieren (Review-F1)

- **Origin:** Kombinierter Review TASK-026..030, Finding F1 (Kritisch)
- **Entscheidung Mensch:** Variante (a) — HAL-Init nutzt dieselben MOD_*-Tags als Owner wie das Feature-Modulsystem

- **Status:** done

## Kontext / Problem

`setup()` in `main.cpp` ruft `hal_esp32_init_all()` auf (Zeile 461), das Pins unter **Legacy-Owner-Tags** claimt (z. B. `"imu-int"`, `"imu-cs"`, `"eth-sck"` — siehe `hal_impl.cpp` Funktionen `claimCommonInitPins()`, `claimImuSteerInitPins()`, `claimEthPins()`, `claimGnssUartPins()`). Danach werden in `setup()` `moduleActivate(MOD_IMU)`, `moduleActivate(MOD_ETH)` etc. aufgerufen (Zeilen 576-584), die dieselben Pins unter **MOD_*-Tags** claimen wollen (z. B. `"MOD_IMU"`, `"MOD_ETH"` — siehe `modules.cpp` Funktion `featureOwnerTag()`).

Da `moduleActivate()` in `modules.cpp` (Zeile 483) bei **verschiedenen** Ownern einen harten Konflikt auslöst (ADR-HAL-001: *"Pin-Konflikte werden standardmäßig hart behandelt"*), scheitern die Standard-Aktivierungen in `setup()` auf dem ESP32-S3.

**Auf dem ESP32 Classic ist das Problem nicht akut** (IMU-Pins sind alle -1, `moduleActivate(MOD_IMU)` wird mit `compiled=false` beendet), aber der Codepfad ist semantisch falsch.

### Betroffene Pin-Claims (hal_impl.cpp)

| Funktion | Legacy Owner-Tag | Sollte sein (MOD_*) | Pins |
|----------|-----------------|---------------------|------|
| `claimCommonInitPins()` | `"safety-input"` | `"MOD_SAFETY"` | SAFETY_IN=4 |
| | `"sensor-spi-sck"` | `"MOD_IMU"` | SENS_SPI_SCK=47 |
| | `"sensor-spi-miso"` | `"MOD_IMU"` | SENS_SPI_MISO=21 |
| | `"sensor-spi-mosi"` | `"MOD_IMU"` | SENS_SPI_MOSI=38 |
| `claimImuSteerInitPins()` | `"imu-int"` | `"MOD_IMU"` | IMU_INT=46 |
| | `"imu-rst"` | `"MOD_IMU"` | IMU_RST=41 |
| | `"imu-wake"` | `"MOD_IMU"` | IMU_WAKE=15 |
| | `"imu-cs"` | `"MOD_IMU"` | CS_IMU=40 |
| | `"steer-angle-cs"` | `"MOD_ADS"` | CS_STEER_ANG=18 |
| | `"actuator-cs"` | `"MOD_ACT"` | CS_ACT=16 |
| `claimEthPins()` | `"eth-sck"` | `"MOD_ETH"` | ETH_SCK=10 |
| | `"eth-miso"` | `"MOD_ETH"` | ETH_MISO=11 |
| | `"eth-mosi"` | `"MOD_ETH"` | ETH_MOSI=12 |
| | `"eth-cs"` | `"MOD_ETH"` | ETH_CS=9 |
| | `"eth-int"` | `"MOD_ETH"` | ETH_INT=13 |
| | `"eth-rst"` | `"MOD_ETH"` | ETH_RST=14 |
| `claimGnssUartPins()` | `"gnss-rtcm-rx"` | `"MOD_GNSS"` | UART RX |
| | `"gnss-rtcm-tx"` | `"MOD_GNSS"` | UART TX |

### Problem mit Sensor-SPI-Pins

Die Sensor-SPI-Pins (SCK=47, MISO=21, MOSI=38) werden von IMU, ADS und ACT gemeinsam genutzt (drei Module teilen sich denselben SPI-Bus). Der korrekte Owner für diese Pins ist nicht eindeutig einem einzelnen Modul zuzuordnen. Per ADR-HAL-001 muss ein Claim einen eindeutigen Owner haben.

**Lösung:** Die SPI-Bus-Pins gehören zum HAL und werden unter einem dedizierten Owner `"HAL_SENSOR_SPI"` geclaimt (kein MOD_*-Tag). Die Sensor-SPI-Bus-Claims werden nicht von `moduleActivate()` verwaltet, sondern vom HAL-Init. Das Feature-Modulsystem claimt nur die CS-Pins und modulspezifischen Pins (INT, RST, WAKE).

## Akzeptanzkriterien

1. `hal_impl.cpp`: Alle Pin-Claims in `claimCommonInitPins()`, `claimImuSteerInitPins()`, `claimEthPins()`, `claimGnssUartPins()` verwenden **MOD_***-Tags als Owner (Ausnahme: Sensor-SPI-Bus-Pins unter `"HAL_SENSOR_SPI"`)
2. `moduleActivate(MOD_IMU)` in `setup()` succeeds auf ESP32-S3 (kein Konflikt mit HAL-Init-Claims)
3. `moduleActivate(MOD_ETH)` in `setup()` succeeds auf ESP32-S3
4. Alle relevanten Build-Profile kompilieren fehlerfrei:
   - S3: `T-ETH-Lite-ESP32S3` (Basis), `profile_comm_only`, `profile_sensor_front`, `profile_actor_rear`
   - Classic: `T-ETH-Lite-ESP32` (Basis), `profile_full_steer_ntrip` (nach TASK-034: `profile_ntrip_classic`)
   - Hinweis: Es existiert kein `profile_full_steer` für den S3 — die Legacy-Tags betreffen alle Profile gleichermaßen über `hal_esp32_init_all()`
5. Boot-Log zeigt keine `"Pin claim conflict"` Meldungen für Module, die in `setup()` aktiviert werden
6. ESP32 Classic: Keine Regression (Pins=-1 werden weiterhin korrekt übersprungen)

## Scope (in)

- `src/hal_esp32/hal_impl.cpp`: Owner-Tags in allen `claimXxxPins()` Funktionen ändern
- `src/main.cpp`: Kommentar bei `moduleActivate()`-Aufrufen aktualisieren
- Ggf. `src/logic/modules.cpp`: Wenn SPI-Bus-Pins nicht mehr von einzelnen Modulen geclaimt werden, die FEAT_PINS_*-Arrays prüfen (SPI-SCK/MISO/MOSI sollten nicht in FEAT_PINS_IMU/-ADS/-ACT auftauchen, da sie vom HAL geclaimt werden)

## Nicht-Scope (out)

- Keine Änderung an `moduleActivate()`-Logik
- Kein Übergangs-ADR (Variante (a) ist die saubere Lösung, ADR-003 wird voll konform eingehalten)
- Keine Änderung an ESP32 Classic Board-Profil (dort keine akute Auswirkung)

## Verifikation / Test

- `pio run -e T-ETH-Lite-ESP32S3` — Basis-Build muss kompilieren
- `pio run -e profile_sensor_front` — S3-Modulprofil (IMU + ADS aktiv) muss kompilieren
- `pio run -e profile_actor_rear` — S3-Modulprofil (ACT aktiv) muss kompilieren
- `pio run -e T-ETH-Lite-ESP32` — Classic-Basis muss kompilieren
- `pio run -e profile_full_steer_ntrip` — Classic-NTRIP-Profil muss kompilieren (nach TASK-034: `profile_ntrip_classic`)
- Boot-Log: Keine `"Pin claim conflict"` Meldungen
- `rg "Pin claim conflict" src/` — sollte nur noch in Fehlerfall-Codes auftauchen

## Relevante ADRs

- **ADR-003** (Feature-Modulsystem): *"Bereits vorhandene Legacy-Claims dürfen nur mit explizit dokumentierter Ausnahme akzeptiert werden."* → Wird durch Harmonisierung erfüllt.
- **ADR-HAL-001** (Pin-Konflikt-Politik): *"Ein Kommentar im Boardprofil ersetzt keine technische Konfliktbehandlung."* → Hard arbitration bleibt bestehen, Owner-Tags werden harmonisiert.

## Invarianten

- Die Pin-Claim-Tabelle hat feste Kapazität (32 Einträge). Die Harmonisierung ändert nur die Owner-Tags, nicht die Anzahl der Claims.
- Negative Pins (< 0) werden weiterhin übersprungen (`if (pin < 0) continue;`)

## Known Traps

1. **SPI-Bus-Pins sind geteilt:** SCK=47, MISO=21, MOSI=38 werden von IMU, ADS und ACT gemeinsam genutzt. Sie dürfen nicht mehreren MOD_*-Ownern gleichzeitig gehören. Lösung: Dedizierter Owner `"HAL_SENSOR_SPI"`.
2. **ESP32 Classic Pins=-1:** Alle Sensor-Pins sind -1 auf dem Classic. Die Claims werden übersprungen, aber die Owner-Tags müssen trotzdem korrekt sein, um zukünftige Board-Erweiterungen nicht zu brechen.
3. **Claim-Reihenfolge:** HAL-Init muss VOR `moduleActivate()` laufen. Das ist bereits der Fall (`setup()` Zeile 461 vs. 576-584).
4. **`hal_pin_claim_owner()` Returns:** `featureOwnerTag()` in `modules.cpp` (Zeile 337-348) liefert die MOD_*-Tags. Diese Strings müssen exakt mit den Tags in `hal_impl.cpp` übereinstimmen.

## Rejected Alternatives

- **Variante (b) — moduleActivate erkennt Legacy-Claims:** Würde ADR-HAL-001 schwächen ("Claim ist nicht automatisch konfliktfrei") und erfordert einen neuen Übergangs-ADR. Abgelehnt vom Menschen.
- **Variante (c) — Init-Reihenfolge tauschen:** Riskant, da HAL-Init funktionierende Hardwareinitialisierung benötigt. Abgelehnt vom Menschen.

## Merge Risk

- **Mittel:** Ändert Owner-Strings in der HAL, was sich auf die Claim-Arbitrierung auswirkt. Alle Builds müssen getestet werden.
- **Betroffene Builds:** Alle Profile, die `hal_esp32_init_all()` verwenden (d. h. alle Nicht-Bringup-Profile).

## Classification

- **category:** platform_reuse
- **priority:** high
- **delivery_mode:** firmware_only
- **exclusive_before:** TASK-031 muss VOR Merge von TASK-026..030 abgeschlossen sein
- **parallelizable_after:** Parallel mit TASK-032, TASK-033, TASK-034, TASK-035

## Owner

- **Assigned:** KI-Entwickler
- **Status:** todo
