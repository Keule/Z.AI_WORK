Entwickler-Report für Task TASK-037

Entwickler: Super Z (KI-Entwickler, agents.md §4.2)
Task-ID: TASK-037

Checkliste (Pflichtfelder)

- [x] Zusammenfassung ausgefüllt
- [x] Geänderte Dateien vollständig aufgelistet
- [x] Tests / Build dokumentiert (inkl. Ergebnis)
- [x] Offene Fragen / Probleme dokumentiert (oder explizit "keine")

Zusammenfassung

TASK-037: features.h aufräumen und vereinfachen.

Das alte features.h (246 Zeilen) enthielt 7 vermischte Konzepte:
1. Profile-Flags (FEAT_PROFILE_COMM_ONLY etc.)
2. AOG-Domain-Features (FEAT_STEER_SENSOR, FEAT_MACHINE_ACTOR etc.)
3. Abgeleitete Capabilities (FEAT_CAP_SENSOR_SPI2, FEAT_CAP_GNSS_UART_MIRROR)
4. Build-Modi (FEAT_GNSS_BUILDUP, FEAT_IMU_BRINGUP)
5. NTRIP-spezifische Flags
6. Legacy-Alias-Schicht (feat::comm(), feat::sensor() etc.)
7. feat:: C++ Namespace

Aufgabenstellung: Alle capabilities, profile-flags, AOG-domain-roles, legacy-aliase und build-modi streichen. Nur 9 flache Hardware-Feature-Flags behalten. Profile werden zu reinen platformio.ini [env:] Blöcken.

Ergebnis: features.h reduziert von 246 auf 51 Zeilen. 9 flache Hardware-Flags (FEAT_IMU, FEAT_ADS, FEAT_ACT, FEAT_ETH, FEAT_GNSS, FEAT_NTRIP, FEAT_SD, FEAT_SAFETY, FEAT_LOGSW). FEAT_NORM-Makro für 0/1-Normierung. feat:: Namespace mit inline constexpr bool-Helpers. Profile in platformio.ini als [env:profile_*] Blöcke mit direkten -DFEAT_* Flags.

Umsetzung in 3 Inkrementen:

Increment 1 (neu in dieser Session abgeschlossen):
- features.h: Komplett-Neuschreibung (246 -> 51 Zeilen)
- platformio.ini: Build-Modi (gnss_buildup, imu_bringup) entfernt, 4 Profil-Envs erstellt
- tools/smoke_matrix.cpp: Alle Feature-Namen auf neue Namensgebung migriert
- tools/run_test_matrix.py: Alle Feature-Namen und Env-Namen migriert

Increment 2 (neu in dieser Session abgeschlossen):
- src/logic/modules.cpp: feat::comm()->feat::eth(), feat::sensor()->feat::ads(), feat::actor()->feat::act(), feat::control()->feat::act() (AOG-Modul-Ermittlung behält eigene Logik)
- src/hal_esp32/hal_impl.cpp: feat::sensor()->feat::ads(), feat::actor()->feat::act(), FEAT_CAP_SENSOR_SPI2 Guards entfernt
- src/main.cpp: NTRIP-Guards FEAT_ENABLED(FEAT_NTRIP)->FEAT_ENABLED(FEAT_COMPILED_NTRIP), feat::control()->feat::act()&&feat::safety(), feat::sensor()->feat::ads(), FEAT_GNSS_UART_MIRROR Log-Text korrigiert, Build-Modi-Pfade (GNSS_BUILDUP, IMU_BRINGUP) bereits in vorheriger Session entfernt
- src/logic/ntrip.h: FEAT_ENABLED(FEAT_NTRIP)->FEAT_ENABLED(FEAT_COMPILED_NTRIP)
- src/logic/ntrip.cpp: FEAT_ENABLED(FEAT_NTRIP)->FEAT_ENABLED(FEAT_COMPILED_NTRIP)
- src/hal_esp32/sd_logger_esp32.cpp: FEAT_ENABLED(FEAT_NTRIP)->FEAT_ENABLED(FEAT_COMPILED_NTRIP)
- src/logic/imu.cpp: FEAT_IMU_BRINGUP #if defined() Guard entfernt, k_imu_bringup_mode = false hardcoded

Increment 3 (Verification):
- Vollständiger rg Sweep über src/ und tools/: KEINE Treffer für alte Feature-Namen
- PlatformIO Build konnte in dieser Umgebung nicht ausgeführt werden (PIO nicht installiert)
- Manuelle Code-Review: Alle Guards konsistent, keine widersprüchlichen Referenzen

Geänderte Dateien

| Datei | Änderung |
|-------|---------|
| src/logic/features.h | Komplett-Neuschreibung: 246 -> 51 Zeilen |
| platformio.ini | Build-Modi entfernt, 4 Profil-Envs erstellt, FEAT_PROFILE_* Flags entfernt |
| tools/smoke_matrix.cpp | Feature-Namen migriert |
| tools/run_test_matrix.py | Feature-Namen und Env-Namen migriert |
| src/logic/modules.cpp | feat::comm()->feat::eth(), feat::sensor()->feat::ads(), feat::actor()->feat::act() |
| src/hal_esp32/hal_impl.cpp | feat::sensor()->feat::ads(), feat::actor()->feat::act(), FEAT_CAP_* Guards entfernt |
| src/main.cpp | NTRIP-Guards migriert, feat::control/feat::sensor migriert, Log-Text korrigiert |
| src/logic/ntrip.h | FEAT_ENABLED(FEAT_NTRIP)->FEAT_ENABLED(FEAT_COMPILED_NTRIP) |
| src/logic/ntrip.cpp | FEAT_ENABLED(FEAT_NTRIP)->FEAT_ENABLED(FEAT_COMPILED_NTRIP) |
| src/hal_esp32/sd_logger_esp32.cpp | FEAT_ENABLED(FEAT_NTRIP)->FEAT_ENABLED(FEAT_COMPILED_NTRIP) |
| src/logic/imu.cpp | FEAT_IMU_BRINGUP Guard entfernt |

Tests / Build

- rg Sweep (AC-Check): `rg "FEAT_STEER_SENSOR|...|feat::pid" src/ tools/ --count` -> 0 Treffer in src/ und tools/
- Vollständiger Sweep auf alle alten Feature-Namen: 0 Treffer in src/, tools/, include/
- PlatformIO Build: Nicht ausführbar (PIO nicht in dieser Umgebung installiert). Der Entwickler muss lokal bauen.
- Smoke Tests: smoke_matrix.cpp und run_test_matrix.py wurden migriert, können nach lokalem PIO-Build verifiziert werden.

Offene Fragen / Probleme

1. **Lokaler Build erforderlich**: Der finale Build-Test (alle 4 Profile + beide Board-Varianten) muss auf dem Entwickler-Rechner durchgeführt werden.
2. **GNSS_BUILDUP-Doku veraltet**: docs/process/GNSS_BUILDUP.md referenziert noch alte Build-Modi. Dies ist historische Doku und wurde bewusst nicht geändert (agents.md §6: Reports/Doku nur bei explizitem Auftrag).
3. **Historische Reports**: reports/ und backlog/tasks/ enthalten noch alte Feature-Namen in Snapshot-Dokus. Das ist korrekt (historische Kontextdokumentation).
