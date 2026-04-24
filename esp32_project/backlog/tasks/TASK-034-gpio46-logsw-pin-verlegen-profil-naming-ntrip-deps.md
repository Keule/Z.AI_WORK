# Task: GPIO-46 LOG_SWITCH_PIN verlegen (GPIO3) und Profil umbenennen (Review-F4, F5)

- **Origin:** Kombinierter Review TASK-026..030, Findings F4 (Mittel), F5 (Mittel)
- **Entscheidung Mensch/Planer:** LOG_SWITCH_PIN auf GPIO 3 verlegen; NTRIP braucht nicht zwingend GNSS (kein Dependency-Fix in diesem Task)

## Kontext / Problem

### F4: GPIO-46 Konflikt — IMU_INT vs LOG_SWITCH_PIN

Auf dem ESP32-S3 wird GPIO-46 von zwei Feature-Modulen beansprucht:
- `IMU_INT = 46` (BNO085 Interrupt-Pin, MOD_IMU)
- `LOG_SWITCH_PIN = 46` (SD-Logging Schalter, MOD_LOGSW)

Per ADR-HAL-001 muss ein Kommentar keine technische Konfliktbehandlung ersetzen. Entscheidung: LOG_SWITCH_PIN wird auf **GPIO 3** verlegt.

**Hinweis ESP32 Classic:** Auf dem ESP32 Classic ist `LOG_SWITCH_PIN = PIN_BT = 0`. Keine Änderung nötig.

### F5: Irreführender Profilname in platformio.ini

Das Profil `[env:profile_full_steer_ntrip_esp32]` trägt `-DFEAT_PROFILE_FULL_STEER` als Flag, enthält aber kein Steer-Feature (nur COMM + GNSS + NTRIP auf ESP32 Classic).

**Umbenennung:** `profile_full_steer_ntrip_esp32` → `profile_ntrip_classic` und `-DFEAT_PROFILE_FULL_STEER` entfernen.

## Akzeptanzkriterien

### F4:
1. LOG_SWITCH_PIN auf ESP32-S3 ist auf GPIO 3 verlegt und kollidiert nicht mehr mit IMU_INT (46)
2. `moduleActivate(MOD_IMU)` und `moduleActivate(MOD_LOGSW)` können beide erfolgreich aktivieren (kein Konflikt)
3. ESP32 Classic: Keine Änderung (LOG_SWITCH_PIN bleibt PIN_BT=0)

### F5:
1. Profil `[env:profile_full_steer_ntrip_esp32]` umbenannt zu `[env:profile_ntrip_classic]`
2. `-DFEAT_PROFILE_FULL_STEER` aus dem Profil entfernt
3. Kein anderes Profil referenziert den alten Namen

### Allgemein:
1. Alle Build-Profile kompilieren fehlerfrei
2. `rg "GPIO 46" include/` zeigt IMU_INT=46 aber nicht LOG_SWITCH_PIN=46

## Scope (in)

- `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h`:
  - LOG_SWITCH_PIN auf GPIO 3 ändern
  - FEAT_PINS_LOGSW aktualisieren
  - Kommentar "CONFLICT with IMU_INT!" entfernen
- `platformio.ini`:
  - `[env:profile_full_steer_ntrip_esp32]` → `[env:profile_ntrip_classic]`
  - `-DFEAT_PROFILE_FULL_STEER` entfernen
- Ggf. Dokumentation (`README.md` oder Board-Doc) aktualisieren

## Nicht-Scope (out)

- Keine Änderung an ESP32 Classic LOG_SWITCH_PIN (PIN_BT=0 ist korrekt)
- Keine Änderung an NTRIP-Dependencies (ETH-only bleibt so)
- Keine Änderung am Modulsystem-Code (`modules.cpp`)
- Kein Hardware-Redesign (nur Pin-Neuzuweisung im Profil)

## Verifikation / Test

- `pio run -e T-ETH-Lite-ESP32S3` — S3-Basis muss kompilieren (GPIO-3 Referenz im Board-Profil)
- `pio run -e profile_sensor_front` — S3-Modulprofil (MOD_IMU + MOD_LOGSW aktiv, GPIO-Konflikt-Test)
- `pio run -e profile_ntrip_classic` (umbenannt) — muss kompilieren
- `pio run -e profile_full_steer_ntrip` — Classic-NTRIP-Profil muss kompilieren (unverändert, extends Classic)
- `rg "profile_full_steer_ntrip_esp32" platformio.ini` — keine Treffer (alter Name entfernt)
- `rg "LOG_SWITCH_PIN.*46" include/` — keine Treffer (GPIO-46 Konflikt behoben)

## Relevante ADRs

- **ADR-HAL-001** (Pin-Konflikt-Politik): Konflikte nicht per Kommentar lösen.
- **ADR-003** (Feature-Modulsystem): Pin-Konflikte nicht stillschweigend tolerieren.

## Invarianten

- LOG_SWITCH_PIN bleibt active LOW + internal pull-up (nur GPIO ändert sich)
- NTRIP-Dependencies bleiben unverändert (ETH-only)

## Known Traps

1. **ESP32-S3 Pin-Restriktionen:** GPIO 26-37 PSRAM-reserviert, GPIO 38-42 output-only.
2. **Profil-Namensänderung:** CI/CD, Skripte und Doku müssen ggf. den neuen Namen nutzen.
3. **Strapping-Pin-Risiko:** GPIO 3 muss im Zielsetup verifiziert werden; vor finalem Compile kann bei Bedarf ein anderer freier Pin gesetzt werden.

## Merge Risk

- **Niedrig bis Mittel:** Pin-Änderung betrifft nur ESP32-S3 Board-Profil; Profil-Umbenennung ist kosmetisch.

## Classification

- **category:** platform_reuse
- **priority:** medium
- **delivery_mode:** firmware_only
- **exclusive_before:** Keine
- **parallelizable_after:** Parallel mit allen anderen TASK-03x

## Owner

- **Assigned:** KI-Entwickler
- **Status:** todo
