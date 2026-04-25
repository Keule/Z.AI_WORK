Entwickler-Report: TASK-026..030 Review-Fixes

Entwickler: Super Z (Z.ai)
Task-ID: TASK-026-030-fix
Branch: fix/tasks-26-30-review (based on zai/complete-tasks-26-30)
Datum: 2026-04-21

Checkliste (Pflichtfelder)

- [x] Zusammenfassung ausgefuellt
- [x] Geaenderte Dateien vollstaendig aufgelistet
- [x] Tests / Build dokumentiert (inkl. Ergebnis)
- [x] Offene Fragen / Probleme dokumentiert (oder explizit "keine")

Zusammenfassung

Behebung von 5 Mangeln aus dem Review der TASK-026..030 Umsetzung auf dem ZAI-Branch. Alle Aenderungen sind minimalinvasiv und konservativ; keine Architektur wurde umbaut.

Geaenderte Dateien

1. include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h
   - FEAT_DEPS_NTRIP[] Terminator: 0 -> 0xFF
   - FEAT_DEPS_ACT[] Terminator: 0 -> 0xFF
   - Kommentar aktualisiert ("0xFF terminated" statt "0 terminated")

2. include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h
   - Gleiche Aenderungen wie ESP32-S3 Profil

3. src/logic/modules.h
   - FeatureModuleInfo::deps Dokumentation: "0xFF terminated" statt "0 terminated"

4. src/logic/modules.cpp
   - moduleActivate() Dependency-Iteration: != 0 -> != 0xFF
   - moduleActivate() Pin-Claim: harte Konflikt-Erkennung per ADR-HAL-001
     - hal_pin_claim_owner() abfragen statt nur hal_pin_claim_check()
     - Gleicher Owner -> idempotent (OK)
     - Anderer Owner -> CONFLICT-Log, Pin-Release-Rollback, return false
   - Kommentar aktualisiert: "hard conflict detection per ADR-HAL-001"

5. src/hal/hal.h
   - Neue Deklaration: const char* hal_pin_claim_owner(int pin);

6. src/hal_esp32/hal_impl.cpp
   - Neue Implementierung: hal_pin_claim_owner() basierend auf pinClaimFind()

7. src/main.cpp
   - GNSS-Buildup NTRIP-Init: Demo-Fallbacks ("caster.example.com", "VRS") entfernt
   - Leere Config fuehrt zu Log-Warnung "NTRIP not configured"
   - NTRIP bleibt inaktiv (ntripTick() IDLE-State prueft host/mountpoint)

8. src/hal_esp32/sd_logger_esp32.cpp
   - sdLoggerPsramInit() Kommentar erweitert:
     - "buffer selection happens ONCE at init"
     - "NOTE: SD_SPI_BUS bus is shared... blocks sensor SPI for ~50-200 ms... NOT fully decoupled"

Tests / Build

- Static code validation:
  - rg '"hardware_pins\.h"' src/ -> keine Treffer (Mangel 5 erfuellt)
  - rg 'caster\.example|"VRS"' src/ -> kein Demo-Fallback im Code (Mangel 3 erfuellt)
  - rg 'deps\[.*\] != 0\b' src/ -> keine Treffer (Terminator korrigiert)
  - deps[].!= 0xFF korrekt in modules.cpp:447
- PlatformIO Build nicht verfuegbar in dieser Umgebung.
- Logische Traces:
  - moduleActivate(MOD_ACT) ohne vorherige IMU/ADS-Aktivierung -> fehlgeschlagen (Dependency MOD_IMU=0 wird geprueft dank 0xFF-Terminator)
  - moduleActivate(MOD_IMU) gefolgt von moduleActivate(MOD_LOGSW) -> Konflikt auf GPIO 46 (IMU_INT vs LOG_SWITCH_PIN) -> fehlgeschlagen (harte Arbitrierung)

Offene Fragen / Probleme

1. HAL-Init-Pfad-Konflikt mit harter Arbitrierung:
   Der HAL-Init-Pfad (hal_esp32_init_all()) claimt Pins unter Legacy-Ownern
   wie "imu-cs", "eth-cs", etc. Die moduleActivate()-Funktion claimt Pins unter
   "MOD_IMU", "MOD_ETH", etc. Bei gleichzeitigem Einsatz beider Systeme kann es
   zu harten Konflikten kommen, da die Owner-Strings unterschiedlich sind.
   
   In der aktuellen setup()-Sequenz wird moduleActivate() NACH hal_esp32_init_all()
   aufgerufen. Die HAL-Init-Claims existieren also bereits. Mit der neuen harten
   Logik wuerde moduleActivate(MOD_IMU) auf GPIO 46 mit dem Legacy-Owner
   konfliktieren, wenn der HAL GPIO 46 unter einem anderen Namen geclaimt hat.
   
   Dies ist ein bekannter Uebergangskonflikt (ADR-003: "Uebergang vom Legacy-System
   kann temporaer doppelte Strukturen erzeugen"). Die Loesung erfordert entweder:
   (a) HAL-Init verwendet dieselben MOD_* Owner-Tags, oder
   (b) moduleActivate() akzeptiert Legacy-Claims mit dokumentierter Ausnahme.
   
   Option (b) wurde bewusst NICHT implementiert, da ADR-HAL-001 "harte Politik"
   fordert und eine Ausnahme dokumentiert werden muesste (z.B. als Uebergangs-ADR).
   Der aktuelle Code wuerde bei GPIO 46 einen harten Konflikt melden, was korrekt
   aber ggf. unerwartet ist, da GPIO 46 von beiden Systemen beansprucht wird.
   
   EMPFEHLUNG: Mensch/Planer entscheiden, ob (a) oder (b) gewuenscht ist.

2. Kein PlatformIO Build verfuegbar:
   Die Compile-Pruefung konnte nicht durchgefuehrt werden, da PlatformIO in dieser
   Umgebung nicht installiert ist. Alle Aenderungen sind syntaktisch geprueft
   (Grep-Validierung, logische Traces), aber ein vollstaendiger Build steht noch aus.

Nicht behobene Punkte (mit Begruendung)

Keine. Alle 5 Mangel wurden adressiert.
