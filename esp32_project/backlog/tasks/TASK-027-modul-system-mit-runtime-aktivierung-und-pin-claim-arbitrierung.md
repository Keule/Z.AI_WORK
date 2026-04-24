# TASK-027 Modul-System mit Runtime-Aktivierung und Pin-Claim-Arbitrierung

- **ID**: TASK-027
- **Titel**: Modul-System mit Runtime-Aktivierung/Deaktivierung, Pin-Gruppen und dreiwertigem Status
- **Status**: done
- **Priorität**: high
- **Komponenten**: src/logic/modules.cpp, src/logic/modules.h, include/board_profile/*, src/logic/features.h, src/hal_esp32/hal_impl.cpp, src/main.cpp
- **Dependencies**: TASK-026
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer
- **Epic**: EPIC-003

- **classification**: dependent
- **exclusive_before**: [TASK-026]
- **parallelizable_after**: []

- **Origin**:
  Nutzer-Anforderungen (zusammengefasst aus Diskussion):
  1. "Wie waere dein Vorschlag mit minimalen Aenderungen a) beim Kompilieren zu bestimmen, welche Features nutzbar sind b) welche Hardware aktiviert/deaktiviert ist zur Runtime?"
  2. "Dein Vorschlag in features.h die Pins zuzuweisen funktioniert nicht mit hardcoded Pin-Nummern. Je nach Board sind die Devices unterschiedlich angeschlossen."
  3. "Die aktiven Module in der soft_config sollten ALLE Module erfassen, auch die, die per fw_config nicht in die Firmware compiliert sind (1=enabled, 0=disabled, -1=nicht in fw)."
  4. GPIO-Konflikt vermeiden, insbesondere GPIO 46 (IMU_INT vs LOG_SWITCH_PIN).

- **Diskussion**:
  - Direkt: https://chat.z.ai/c/d6f6eb9b-9217-401b-bb23-08e8c0fbca69
  - Shared: https://chat.z.ai/s/a858dd17-02e3-416c-a123-649830256a4e

- **Kontext/Problem**:
  Aktuell ist `modules.cpp` (`AogModuleInfo`) statisch nach Init — `enabled` wird nie geaendert, `hw_detected` wird nie geaendert. Es gibt keine Moeglichkeit, Hardware zur Runtime zu aktivieren oder zu deaktivieren. Pin-Gruppen pro Feature sind nicht definiert — das Board-Profil hat nur Einzel-Pins. GPIO 46 wird sowohl als IMU_INT als auch als LOG_SWITCH_PIN verwendet, ohne dass das Claim-System den Konflikt erkennt (LOG_SWITCH_PIN umgeht das Claim-System). Die Modul-Liste ist nicht vollstaendig — sie enthaelt nur kompilierte Module, der User sieht aber nicht, was auf diesem Board grundsaetzlich moeglich waere.

- **Scope (in)**:
  - **Board-Profile erweitern**: `FEAT_PINS_*` Arrays pro Feature in jedem Board-Profil definieren (Pin-Gruppen, -1 terminiert). `FEAT_DEPS_*` fuer Feature-Abhaengigkeiten (NTRIP→ETH, ACT→IMU+ADS).
  - **ModuleInfo umbauen**: Statt `enabled`/`hw_detected` → `compiled` (Build-Flag && Pins verfuegbar), `active` (Runtime-mutable), `hw_detected` (Boot-Detection). `pins[]` Array zeigt auf `FEAT_PINS_*` aus Board-Profil. `deps[]` zeigt auf `FEAT_DEPS_*`.
  - **ModState enum**: `MOD_UNAVAILABLE = -1`, `MOD_OFF = 0`, `MOD_ON = 1`.
  - **Vollstaendige Modul-Liste**: `g_modules[]` enthaelt IMMER alle moeglichen Module (MOD_COUNT). Nicht kompilierte Module bleiben `MOD_UNAVAILABLE`. Pin nicht verdrahtet (-1) → ebenfalls `MOD_UNAVAILABLE`.
  - **moduleActivate() / moduleDeactivate()**: Prueft Pin-Claim-Konflikte via bestehendem `pinClaimFind()`. Aktiviert Hardware bei Erfolg. Gibt Pins bei Deaktivierung frei.
  - **Abhaengigkeits-Check**: moduleActivate("NTRIP") prueft, ob ETH aktiv ist. moduleActivate("ACT") prueft, ob IMU und ADS aktiv sind.
  - **modulesInit()**: Laedt `compiled` aus fw_config (Build-Flags + Pin-Verfuegbarkeit). Setzt `default_state` (constexpr, spaeter cfg::*). Aktiviert alle `MOD_ON` Module mit Pin-Claim.
  - **Pin-Claim-System erweitern**: `pinClaimRelease()` hinzufuegen. `LOG_SWITCH_PIN` ins Claim-System integrieren.
  - **Init-Reihenfolge in main.cpp**: moduleActivate() Aufrufe in definierter Prioritaetsreihenfolge.

- **Nicht-Scope (out)**:
  - `soft_config.h` erstellen (folgt in TASK-028) — Default-Werte werden in diesem Task als constexpr direkt in modules.cpp definiert.
  - Task-Architektur-Aenderungen (controlTask/commTask/maintTask) — folgt in TASK-029.
  - NTRIP-spezifische Aenderungen — folgt in TASK-030.

- **files_read**:
  - `include/fw_config.h` (nach TASK-026)
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h`
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h`
  - `src/logic/features.h`
  - `src/logic/modules.cpp`
  - `src/logic/global_state.h`
  - `src/hal_esp32/hal_impl.cpp` (Pin-Claim-System)
  - `src/main.cpp` (Init-Pfade, setup())
  - `src/hal_esp32/sd_logger_esp32.cpp` (LOG_SWITCH_PIN)

- **files_write**:
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h` (+FEAT_PINS_*, FEAT_DEPS_*)
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h` (+FEAT_PINS_*, FEAT_DEPS_*)
  - `src/logic/modules.cpp` (Komplett-Umbau: ModuleInfo, g_modules[], moduleActivate/Deactivate)
  - `src/logic/modules.h` (NEU oder Umbau: ModState enum, ModuleInfo struct, API)
  - `src/hal_esp32/hal_impl.cpp` (+pinClaimRelease(), LOG_SWITCH ins Claim-System)
  - `src/main.cpp` (moduleActivate-Aufrufe in setup(), Init-Reihenfolge)

- **public_surface**:
  - `src/logic/modules.h` — neue oeffentliche Modul-API (moduleActivate, moduleDeactivate, moduleIsActive, moduleGetState)
  - `include/board_profile/*_board_pins.h` — FEAT_PINS_* Arrays
  - `src/hal_esp32/hal_impl.cpp` — pinClaimRelease() neue HAL-Funktion

- **merge_risk_files**:
  - `src/main.cpp` — Entry-Point, mehrere Tasks greifen hier ein
  - `src/hal_esp32/hal_impl.cpp` — grosse Datei, Init-Pfade
  - `include/board_profile/*_board_pins.h` — Pin-Defines, zentrale Referenz
  - `src/logic/features.h` — Compile-Time-Gating

- **risk_notes**:
  - Umbau von modules.cpp ist zentraler Eingriff — alle Module haengen davon ab. Saubere API noetig, damit bestehende Module (IMU, ADS, Actuator) weiterhin funktionieren.
  - Pin-Claim-Release muss threadsafe sein (commTask Core 0 + controlTask Core 1 greifen evtl. auf gleiche Module zu). Pruefen ob Mutex noetig.
  - GPIO 46 Konflikt: Nach diesem Task muss sichergestellt sein, dass IMU oder LOGSW den Pin exklusiv claimen — nicht beide gleichzeitig.
  - Rueckwaertskompatibilitaet: Bestehende `AogModuleInfo` Referenzen im Code muessen migriert werden.

- **AC**:
  - `ModState` enum mit Werten -1/0/1 existiert in `modules.h`.
  - `ModuleInfo` struct hat Felder: name, compiled, active, hw_detected, pins[], pin_count, deps[].
  - `g_modules[]` enthaelt ALLE Module (IMU, ADS, ACT, ETH, GNSS, NTRIP, SAFETY, LOGSW) — auch nicht kompilierte als `MOD_UNAVAILABLE`.
  - `modulesInit()` setzt `compiled` basierend auf `FEAT_ENABLED(x) && pinsAvailable(FEAT_PINS_*)`.
  - `moduleActivate()` prueft Pin-Claim-Konflikte via `pinClaimFind()`, setzt Claims bei Erfolg, initialisiert Hardware.
  - `moduleDeactivate()` gibt Pin-Claims frei via `pinClaimRelease()`, deinitialisiert Hardware.
  - Abhaengigkeits-Check: NTRIP aktiviert nur wenn ETH aktiv. ACT aktiviert nur wenn IMU+ADS aktiv.
  - GPIO 46: Wenn IMU aktiv, schlaegt LOGSW-Aktivierung fehl (und umgekehrt). Fehler wird per LOGW gemeldet.
  - `pinClaimRelease()` implementiert und in `moduleDeactivate()` genutzt.
  - LOG_SWITCH_PIN ist im Claim-System registriert.
  - `pio run` baut fuer `gnss_buildup` und `gnss_bringup_ntrip`.
  - Bestehende Funktionalitaet bleibt erhalten: IMU-Lesung, ADS-Lesung, Actuator-Steuerung funktionieren wie vorher.

- **verification**:
  - `pio run` fuer alle relevanten Environments.
  - Build mit `FEAT_IMU` deaktiviert → `g_modules[MOD_IMU].compiled == false`, `state == MOD_UNAVAILABLE`.
  - Build mit `FEAT_IMU` aktiviert + ESP32-S3 → `g_modules[MOD_IMU].compiled == true`, `state == MOD_ON`.
  - Build mit `FEAT_IMU` aktiviert + ESP32 Classic → `g_modules[MOD_IMU].compiled == false` (Pins nicht verdrahtet, CS_IMU=-1).
  - Manueller Review: GPIO 46 Konflikt wird erkannt.

- **Links**:
  - `backlog/epics/EPIC-003-platform-and-reuse.md`
  - `backlog/tasks/TASK-024-pin-claims-und-zuweisung-pro-init-pfad.md`
  - `backlog/tasks/TASK-023-capabilities-boot-init-gating-und-onboarding-prompts.md`
