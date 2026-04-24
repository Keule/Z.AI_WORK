# TASK-040 features.h aufräumen und vereinfachen

- **ID**: TASK-040
- **Titel**: features.h aufräumen — Capabilities, Profile-Flags, Legacy-Aliase und Build-Modi streichen
- **Status**: open
- **Priorität**: high
- **Komponenten**: `src/logic/features.h`, `platformio.ini`, `src/main.cpp`, `src/logic/modules.cpp`, `src/hal_esp32/hal_impl.cpp`, `tools/smoke_matrix.cpp`, `tools/run_test_matrix.py`
- **Dependencies**: []
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer
- **Epic**: EPIC-003

- **classification**: dependent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Origin**:
  Nutzeranforderung aus Chat (2026-04-22):
  1. Capabilities sind eigentlich nur Profile — komplett streichen.
  2. Zukünftig nur noch Features (hardware-nahe Funktionseinheiten wie IMU, UART, SD, SPI) und Profile.
  3. Alle Features können sowohl beim Kompilieren als auch zur Runtime aktiviert/deaktiviert werden.
  4. Abhängigkeiten zwischen Features werden nicht automatisch abgeleitet — Nutzer schaltet manuell.
  5. Build-Modi (FEAT_GNSS_BUILDUP, FEAT_IMU_BRINGUP) fliegen komplett raus.
  6. AOG-Domain-Rollen (FEAT_STEER_SENSOR, FEAT_MACHINE_ACTOR etc.) fliegen raus — Code fragt direkt Hardware-Features ab.

- **Diskussion**:
  - Direkt: https://chat.z.ai/c/32dd8e42-f683-4294-b86d-e30515b36891

- **Kontext/Problem**:
  `src/logic/features.h` hat sich über TASK-019..TASK-036 zu einem 245-Zeilen-Konstrukt mit
  7 ineinandergreifenden Konzepten entwickelt:
    1. Profile-Flags (FEAT_PROFILE_COMM_ONLY etc.)
    2. AOG-Domain-Features (FEAT_STEER_SENSOR, FEAT_MACHINE_ACTOR etc.)
    3. Abgeleitete Capabilities (FEAT_CAP_SENSOR_SPI2, FEAT_CAP_GNSS_UART_MIRROR)
    4. 3-Schicht-Normierung (RAW → NORM → Final mit Legacy-Default-On)
    5. Legacy-Alias-Schicht (12 #defines: FEAT_COMM_ETH, FEAT_SENSOR, FEAT_ACTOR etc.)
    6. Build-Modi (FEAT_GNSS_BUILDUP, FEAT_IMU_BRINGUP)
    7. feat:: Namespace-Helper

  Diese Komplexität ist unnötig. Die Architektur soll auf zwei klare Konzepte reduziert werden:
  **Features** (hardware-nahe Funktionseinheiten) und **Profile** (Compile-Time-Presets
  als PlatformIO-Environments). Capabilities, Profile-Flags im Code, AOG-Domain-Rollen,
  Build-Modi und Legacy-Aliase werden komplett entfernt.

  Gleichzeitig werden alle Verbraucher im Codebase auf die neuen direkten Hardware-Feature-Checks
  umgestellt.

---

## Design

### Neue Feature-Flags (hardware-nah, flach)

| Flag | Bedeutung | Alt |
|---|---|---|
| `FEAT_IMU` | BNO085 IMU Sensor | `FEAT_IMU` (gleich) |
| `FEAT_ADS` | ADS1118 Lenkwinkel-ADC | `FEAT_STEER_SENSOR` |
| `FEAT_ACT` | Aktuator-Treiber | `FEAT_STEER_ACTOR` |
| `FEAT_ETH` | Ethernet (W5500 oder RMII) | `FEAT_COMM` / `FEAT_COMM_ETH` |
| `FEAT_GNSS` | GNSS-Empfänger | `FEAT_GNSS` (gleich) |
| `FEAT_NTRIP` | NTRIP-Client | `FEAT_NTRIP` (gleich) |
| `FEAT_SD` | SD-Karte | neu |
| `FEAT_SAFETY` | Safety-Eingang | aus `FEAT_MACHINE_ACTOR` extrahiert |
| `FEAT_LOGSW` | Logging-Schalter | neu |

**Hinweis**: `FEAT_ADS` ersetzt `FEAT_STEER_SENSOR`. Der Name `ADS` ist hardware-nah
(der ADS1118 ist der Lenkwinkel-Sensor). Code der den "Steer Sensor" meint, fragt
künftig `feat::ads()` ab.

### Neue features.h (~35 Zeilen)

```c
#pragma once
// ===================================================================
// Feature Flags — Hardware-nahe Funktionseinheiten
//
// Compile-Time : -DFEAT_XXX  (platformio.ini oder profile-env)
// Runtime       : moduleActivate(MOD_XXX) / moduleDeactivate(MOD_XXX)
//
// Profile = benanntes [env:] in platformio.ini,
//           das mehrere -DFEAT_XXX zusammenfasst.
// ===================================================================

#define FEAT_NORM(flag) ((defined(flag)) ? 1 : 0)

#define FEAT_COMPILED_IMU     FEAT_NORM(FEAT_IMU)
#define FEAT_COMPILED_ADS     FEAT_NORM(FEAT_ADS)
#define FEAT_COMPILED_ACT     FEAT_NORM(FEAT_ACT)
#define FEAT_COMPILED_ETH     FEAT_NORM(FEAT_ETH)
#define FEAT_COMPILED_GNSS    FEAT_NORM(FEAT_GNSS)
#define FEAT_COMPILED_NTRIP   FEAT_NORM(FEAT_NTRIP)
#define FEAT_COMPILED_SD      FEAT_NORM(FEAT_SD)
#define FEAT_COMPILED_SAFETY  FEAT_NORM(FEAT_SAFETY)
#define FEAT_COMPILED_LOGSW   FEAT_NORM(FEAT_LOGSW)

static_assert(FEAT_COMPILED_ETH, "FEAT_ETH ist Pflicht (mindestens Ethernet/UDP).");

#define FEAT_ENABLED(flag)  ((flag) != 0)
#define FEAT_DISABLED(flag) ((flag) == 0)

namespace feat {
inline constexpr bool imu()    { return FEAT_ENABLED(FEAT_COMPILED_IMU); }
inline constexpr bool ads()    { return FEAT_ENABLED(FEAT_COMPILED_ADS); }
inline constexpr bool act()    { return FEAT_ENABLED(FEAT_COMPILED_ACT); }
inline constexpr bool eth()    { return FEAT_ENABLED(FEAT_COMPILED_ETH); }
inline constexpr bool gnss()   { return FEAT_ENABLED(FEAT_COMPILED_GNSS); }
inline constexpr bool ntrip()  { return FEAT_ENABLED(FEAT_COMPILED_NTRIP); }
inline constexpr bool sd()     { return FEAT_ENABLED(FEAT_COMPILED_SD); }
inline constexpr bool safety() { return FEAT_ENABLED(FEAT_COMPILED_SAFETY); }
inline constexpr bool logsw()  { return FEAT_ENABLED(FEAT_COMPILED_LOGSW); }
}  // namespace feat
```

### Profile in platformio.ini

Profile werden zu reinen PlatformIO-Environments. Kein `FEAT_PROFILE_*` Flag mehr im Code.
Die Profile setzen direkt die relevanten `-DFEAT_*` Flags:

```ini
; Profil: Nur Kommunikation (ETH/UDP), keine Sensor-/Aktorik-Pipeline.
[env:profile_comm_only]
extends = env:T-ETH-Lite-ESP32S3
build_flags =
    ${env:T-ETH-Lite-ESP32S3.build_flags}
    -DFEAT_ETH
    -DFEAT_SD

; Profil: Front-Sensorik aktiv (IMU + ADS), keine Aktorik.
[env:profile_sensor_front]
extends = env:T-ETH-Lite-ESP32S3
build_flags =
    ${env:T-ETH-Lite-ESP32S3.build_flags}
    -DFEAT_ETH
    -DFEAT_IMU
    -DFEAT_ADS
    -DFEAT_SD

; Profil: Rear-Aktorik aktiv (ACT), ohne Front-Sensorik.
[env:profile_actor_rear]
extends = env:T-ETH-Lite-ESP32S3
build_flags =
    ${env:T-ETH-Lite-ESP32S3.build_flags}
    -DFEAT_ETH
    -DFEAT_ACT
    -DFEAT_SAFETY
    -DFEAT_SD

; Profil: Vollausbau Steering (ETH + IMU + ADS + ACT + GNSS + NTRIP + SD + SAFETY)
[env:profile_full_steer]
extends = env:T-ETH-Lite-ESP32
build_flags =
    ${env:T-ETH-Lite-ESP32.build_flags}
    -DFEAT_ETH
    -DFEAT_IMU
    -DFEAT_ADS
    -DFEAT_ACT
    -DFEAT_SAFETY
    -DFEAT_GNSS
    -DFEAT_NTRIP
    -DFEAT_SD
    -DFEAT_LOGSW
```

### Verbraucher-Migration (Mapping)

| Datei | Alt | Neu |
|---|---|---|
| `modules.cpp` | `feat::comm()` | `feat::eth()` |
| `modules.cpp` | `feat::sensor()` | `feat::ads()` |
| `modules.cpp` | `feat::actor()` | `feat::act()` |
| `modules.cpp` | `feat::control()` | `feat::act() && feat::safety()` |
| `hal_impl.cpp` | `feat::sensor()` | `feat::ads()` |
| `hal_impl.cpp` | `feat::actor()` | `feat::act()` |
| `hal_impl.cpp` | `feat::imu()` (bereits korrekt) | unverändert |
| `hal_impl.cpp` | `#if FEAT_CAP_SENSOR_SPI2` | Guard entfernen (SPI-Init ist IMU/ADS/ACT-spezifisch) |
| `main.cpp` | `feat::control()` | `feat::act() && feat::safety()` |
| `main.cpp` | `feat::sensor()` | `feat::ads()` |
| `main.cpp` | `#if FEAT_CAP_GNSS_UART_MIRROR` | Guard entfernen — GNSS-Mirror-Code immer compiled wenn `FEAT_GNSS` |
| `main.cpp` | `#if FEAT_CAP_SENSOR_SPI2` | Guard entfernen |
| `main.cpp` | `#if FEAT_ENABLED(FEAT_NTRIP)` | `#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)` |
| `main.cpp` | `FEAT_GNSS_BUILDUP` / `FEAT_IMU_BRINGUP` | komplett entfernen (siehe Increment 2) |
| `ntrip.h/cpp` | `#if FEAT_ENABLED(FEAT_NTRIP)` | `#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)` |
| `sd_logger_esp32.cpp` | `#if FEAT_ENABLED(FEAT_NTRIP)` | `#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)` |
| `smoke_matrix.cpp` | `FEAT_STEER_SENSOR`, `FEAT_MACHINE_ACTOR` | `FEAT_ADS`, `FEAT_ACT`, `FEAT_ETH` |
| `run_test_matrix.py` | `FEAT_PROFILE_*`, alte Feature-Flags | neue Feature-Flags |

---

## Incremente

### Increment 1: features.h neu schreiben + platformio.ini Profile anpassen
- features.h komplett durch neue flache Version ersetzen (~35 Zeilen)
- platformio.ini: alle Profile-Environments umstellen (alte Flags entfernen, neue `-DFEAT_*`)
- Build-Modi `gnss_bringup`, `gnss_bringup_s3`, `profile_ntrip_classic`, `profile_imu_bringup` aus platformio.ini entfernen
- `profile_full_steer_ntrip` in `profile_full_steer` umbenennen und aufräumen
- tools/run_test_matrix.py: neue Feature-Flag-Mapping-Tabelle
- tools/smoke_matrix.cpp: neue Feature-Checks
- **AC**: Kompiliert für alle verbleibenden Profile ohne Warnung

### Increment 2: Verbraucher im Codebase umstellen
- `src/logic/modules.cpp`: `feat::comm()` → `feat::eth()`, `feat::sensor()` → `feat::ads()`,
  `feat::actor()` → `feat::act()`, `feat::control()` → `feat::act() && feat::safety()`
- `src/hal_esp32/hal_impl.cpp`: `feat::sensor()` → `feat::ads()`, `feat::actor()` → `feat::act()`,
  `#if FEAT_CAP_SENSOR_SPI2` Guards entfernen
- `src/main.cpp`:
  - `feat::control()` → `feat::act() && feat::safety()`
  - `feat::sensor()` → `feat::ads()`
  - `#if FEAT_CAP_GNSS_UART_MIRROR` Guard entfernen (GNSS-Mirror immer compiled wenn FEAT_GNSS)
  - `#if FEAT_CAP_SENSOR_SPI2` Guards entfernen
  - `FEAT_GNSS_BUILDUP`/`FEAT_IMU_BRINGUP` Build-Modi komplett entfernen:
    - `s_gnss_buildup_active`, `s_imu_bringup_active` Variablen entfernen
    - `imuBringupModeEnabled()`/`imuBringupInit()`/`imuBringupTick()` Aufrufe entfernen
    - GNSS-buildup-spezifische Code-Pfade in setup() und loop() entfernen
    - `MAIN_GNSS_BUILDUP_*` Konstanten entfernen
    - setup() auf einen einzigen simplen Init-Pfad vereinfachen
  - NTRIP Guards: `#if FEAT_ENABLED(FEAT_NTRIP)` → `#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)`
- `src/logic/ntrip.h`, `src/logic/ntrip.cpp`: NTRIP Guard anpassen
- `src/hal_esp32/sd_logger_esp32.cpp`: NTRIP Guard anpassen
- **AC**: Kein Bezug mehr auf alte Feature-Namen im src/ Verzeichnis

### Increment 3: Verifizierung und Bereinigung
- `rg` sweep nach verbleibenden alten Feature-Namen im gesamten Projekt
- Alle Profile bauen (`pio run -e profile_comm_only`, `-e profile_sensor_front`,
  `-e profile_actor_rear`, `-e profile_full_steer`, `-e T-ETH-Lite-ESP32`, `-e T-ETH-Lite-ESP32S3`)
- Host smoke tests bauen und laufen lassen
- Leere oder überflüssige Konstanten entfernen (z.B. `MAIN_GNSS_BUILDUP_*` Reste)
- **AC**: `rg "FEAT_STEER_SENSOR|FEAT_STEER_ACTOR|FEAT_MACHINE_ACTOR|FEAT_MACHINE_SENSOR|FEAT_PID_STEER|FEAT_CAP_|FEAT_PROFILE_|FEAT_GNSS_BUILDUP|FEAT_IMU_BRINGUP|FEAT_CFG_PROFILE|FEAT_CFG_RAW|FEAT_CFG_PROF|FEAT_CFG_DEFAULT|FEAT_CFG_MODE|FEAT_CFG_MOD|FEAT_COMM\b|FEAT_CONTROL\b|FEAT_SENSOR\b|FEAT_ACTOR\b|FEAT_PID\b|feat::control|feat::sensor|feat::actor|feat::comm|feat::pid"` ergibt keine Treffer in src/ und tools/

---

- **Pflichtlektüre vor Umsetzung**:
  1. `README.md`
  2. `agents.md`
  3. `docs/process/PLAN_AGENT.md`
  4. `docs/process/QUICKSTART_WORKFLOW.md`
  5. `docs/process/GNSS_BUILDUP.md`
  6. `src/logic/features.h` (aktuell — zu ersetzen)
  7. `src/logic/modules.h` (Modul-IDs — müssen mit neuen Features konsistent bleiben)
  8. `src/logic/modules.cpp` (feat::* Verbraucher)
  9. `platformio.ini` (Profile — umzustellen)
  10. dieser Task (TASK-040)

- **files_read**:
  - `src/logic/features.h`
  - `src/logic/modules.h`
  - `src/logic/modules.cpp`
  - `src/main.cpp`
  - `src/hal_esp32/hal_impl.cpp`
  - `src/logic/ntrip.h`
  - `src/logic/ntrip.cpp`
  - `src/hal_esp32/sd_logger_esp32.cpp`
  - `src/logic/imu.cpp`
  - `platformio.ini`
  - `tools/smoke_matrix.cpp`
  - `tools/run_test_matrix.py`
  - `docs/process/GNSS_BUILDUP.md`
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h`
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h`

- **files_write**:
  - `src/logic/features.h` (komplett neu)
  - `platformio.ini` (Profile umstellen, Build-Modi entfernen)
  - `src/logic/modules.cpp` (feat::* Aufrufe ersetzen)
  - `src/main.cpp` (feat::* Aufrufe, Guards, Build-Modi entfernen)
  - `src/hal_esp32/hal_impl.cpp` (feat::* Aufrufe, FEAT_CAP_* Guards entfernen)
  - `src/logic/ntrip.h` (Guard anpassen)
  - `src/logic/ntrip.cpp` (Guard anpassen)
  - `src/hal_esp32/sd_logger_esp32.cpp` (Guard anpassen)
  - `tools/smoke_matrix.cpp` (Feature-Checks anpassen)
  - `tools/run_test_matrix.py` (Profile-Flags anpassen)
  - `backlog/index.yaml` (Task registrieren)

- **public_surface**:
  - `src/logic/features.h` (kompletter Rewrite — neue API)
  - `platformio.ini` (Profile umbenannt, Flags geändert)
  - `tools/run_test_matrix.py` (öffentliche Build-Matrix)

- **merge_risk_files**:
  - `src/logic/features.h` (kompletter Rewrite — jede Include-Datei ist betroffen)
  - `src/main.cpp` (Build-Modi entfernen, feat::* Migration)
  - `src/logic/modules.cpp` (feat::* Migration)
  - `platformio.ini` (Profile restrukturieren)
  - `src/hal_esp32/hal_impl.cpp` (Guard-Entfernung)

- **risk_notes**:
  - `features.h` ist die am häufigsten inkludierte Datei im Projekt. Ein Fehler bricht alle Builds.
  - Build-Modi-Entfernung (`FEAT_GNSS_BUILDUP`, `FEAT_IMU_BRINGUP`) verändert den Boot-Pfad
    in `main.cpp` massiv. setup() und loop() werden deutlich einfacher, aber der
    Umbau muss konsistent sein (keine toten Code-Pfade, keine vergessenen Variablen).
  - `feat::control()` wird an mehreren Stellen als combined Check verwendet
    (aktuell = MACHINE_ACTOR). Die Ersetzung durch `feat::act() && feat::safety()` muss
    semantisch äquivalent sein. Ein Review aller `feat::control()` Aufrufstellen ist zwingend.
  - `FEAT_CAP_SENSOR_SPI2` wird als `#if` Guard verwendet. Die SPI-Init-Logik muss
    ohne diesen Guard korrekt funktionieren (SPI wird ohnehin nur bei Bedarf initialisiert).
  - `#if FEAT_CAP_GNSS_UART_MIRROR` schützt den GNSS-Mirror-Pfad. Nach Entfernung des
    Guards muss der Mirror-Pfad nicht mehr compiled werden wenn kein GNSS-Feature
    aktiv ist. Der Guard muss durch `#if FEAT_ENABLED(FEAT_COMPILED_GNSS)` ersetzt werden,
    NICHT einfach entfernt.
  - smoke_matrix.cpp und run_test_matrix.py müssen synchron mit features.h geändert werden,
    sonst schlagen Host-Tests fehl.
  - profile_full_steer_ntrip benutzt ESP32 (nicht ESP32-S3). Nach dem Cleanup müssen
    beide Board-Varianten bauen.

- **Invarianten**:
  - `FEAT_ETH` ist Pflicht (static_assert).
  - Alle Profile bauen ohne Fehler und ohne Compile-Time-Warning.
  - Kein Feature-Name aus der alten Architektur ist im Code verblieben.
  - Die feat:: Namespace-Helper sind inline constexpr und zur Compile-Time auswertbar.
  - Module-System (`FirmwareFeatureId`, `moduleActivate()` etc.) bleibt unverändert —
    nur die Compile-Time-Flags die `compiled` setzen, ändern sich.

- **Known traps**:
  - `feat::control()` wird im `#if`-Präprozessor-Kontext NICHT funktionieren
    (nur im C++ Code). Alle `#if` Guards müssen `FEAT_ENABLED(FEAT_COMPILED_*)` verwenden.
  - `FEAT_COMM_ETH` Legacy-Alias wird möglicherweise in Board-Profilen oder HAL referenziert.
    Vollständiges `rg` Sweep ist zwingend.
  - Der GNSS-Mirror-Code in main.cpp ist ~150 Zeilen. Er darf nur bei `FEAT_GNSS` compiled
    werden. Guard durch `#if FEAT_ENABLED(FEAT_COMPILED_GNSS)` ersetzen, nicht einfach weg lassen.
  - In setup() gibt es drei Boot-Pfade (normal, IMU bringup, GNSS buildup). Alle drei
    müssen korrekt zu einem einzigen Pfad zusammengefasst werden. Die `hal_esp32_init_gnss_buildup()`
    und `hal_esp32_init_imu_bringup()` HAL-Funktionen werden danach nicht mehr gerufen —
    aber die Funktionssignaturen dürfen nicht gelöscht werden (Breakage im HAL-Header).
    Aufrufe in main.cpp entfernen, HAL-Funktionen als "deprecated but available" lassen
    oder komplett entfernen wenn sie nur in main.cpp gerufen wurden.

- **Rejected alternatives**:
  - AOG-Domain-Rollen als Kombinationen beibehalten (z.B. `feat::steer() = imu && ads && act`):
    verworfen, da Nutzer direkte Hardware-Features abfragen will.
  - Abhängigkeiten automatisch ableiten:
    verworfen, Nutzer schaltet manuell was er braucht.
  - inkrementelles Umbenennen (Alt → Alias → Neu → Alias entfernen):
    verworfen, da die Aufgabe explizit "aufräumen" heißt — Clean Cut ist klarer und
    vermeidet Übergangs-Schrott.

- **AC**:
  - `features.h` enthält nur noch: FEAT_NORM-Makro, FEAT_COMPILED_* Definitionen,
    static_assert für ETH, FEAT_ENABLED/FEAT_DISABLED Helper, feat:: Namespace mit
    inline constexpr Methoden.
  - Kein `FEAT_PROFILE_*`, `FEAT_CAP_*`, `FEAT_CFG_*`, `FEAT_STEER_SENSOR`,
    `FEAT_STEER_ACTOR`, `FEAT_MACHINE_ACTOR`, `FEAT_MACHINE_SENSOR`, `FEAT_PID_STEER`,
    `FEAT_GNSS_BUILDUP`, `FEAT_IMU_BRINGUP`, Legacy-Alias-Schicht oder `FEAT_CFG_DEFAULT_ON`
    mehr im Codebase.
  - `platformio.ini` Profile setzen direkt `-DFEAT_*` Flags ohne `FEAT_PROFILE_*` Zwischenschritt.
  - Build-Modi-Umgebungen (`gnss_bringup`, `gnss_bringup_s3`, `profile_imu_bringup`,
    `profile_ntrip_classic`) sind aus `platformio.ini` entfernt.
  - `feat::eth()` = altes `feat::comm()`, `feat::ads()` = altes `feat::sensor()`,
    `feat::act()` = altes `feat::actor()`.
  - `main.cpp` hat nur noch einen Boot-Pfad (keine Build-Modi-Variablen, kein
    `s_gnss_buildup_active`, kein `s_imu_bringup_active`).
  - GNSS-Mirror-Code ist per `#if FEAT_ENABLED(FEAT_COMPILED_GNSS)` und nicht per
    `FEAT_CAP_GNSS_UART_MIRROR` gegated.
  - Alle verbleibenden Profile bauen erfolgreich:
    `pio run -e T-ETH-Lite-ESP32`
    `pio run -e T-ETH-Lite-ESP32S3`
    `pio run -e profile_comm_only`
    `pio run -e profile_sensor_front`
    `pio run -e profile_actor_rear`
    `pio run -e profile_full_steer`
  - Host smoke tests kompilieren und passieren.
  - `rg` sweep bestätigt: keine alten Feature-Namen mehr in `src/` und `tools/`.

- **verification**:
  ```bash
  # 1. Sweep nach alten Feature-Namen (muss leer sein)
  rg "FEAT_STEER_SENSOR|FEAT_STEER_ACTOR|FEAT_MACHINE_ACTOR|FEAT_MACHINE_SENSOR|FEAT_PID_STEER|FEAT_CAP_|FEAT_PROFILE_|FEAT_GNSS_BUILDUP|FEAT_IMU_BRINGUP|FEAT_CFG_PROFILE|FEAT_CFG_RAW|FEAT_CFG_PROF|FEAT_CFG_DEFAULT|FEAT_CFG_MODE|FEAT_CFG_MOD|FEAT_COMM\b|FEAT_CONTROL\b|FEAT_SENSOR\b|FEAT_ACTOR\b|FEAT_PID\b|feat::control|feat::sensor|feat::actor|feat::comm|feat::pid" src/ tools/ --count

  # 2. Build alle Profile
  pio run -e T-ETH-Lite-ESP32
  pio run -e T-ETH-Lite-ESP32S3
  pio run -e profile_comm_only
  pio run -e profile_sensor_front
  pio run -e profile_actor_rear
  pio run -e profile_full_steer

  # 3. Host smoke tests
  python3 tools/run_test_matrix.py
  ```
