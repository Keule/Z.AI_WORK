# TASK-045 Task-Watchdog-Reset auf ESP32 Classic analysieren und beheben

- **ID**: TASK-045
- **Titel**: Task-Watchdog-Reset auf ESP32 Classic (RMII Ethernet) analysieren und beheben
- **Status**: open
- **Priorität**: high
- **Komponenten**: `sdkconfig.defaults`, `sdkconfig_s3.defaults`, `src/main.cpp`, `src/hal_esp32/sd_logger_esp32.cpp`, `src/logic/ntrip.cpp`, `src/logic/net.cpp`, `src/hal_esp32/hal_impl.cpp`
- **Dependencies**: []
- **delivery_mode**: mixed
- **task_category**: runtime_stability
- **Owner**: ki-planer
- **Epic**: EPIC-001

- **classification**: independent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Origin**:
  Nutzerbeobachtung aus Chat (2026-04-22):
  Build `profile_full_steer` auf ESP32-WROVER-E (LilyGO T-ETH-Lite-ESP32, 4 MB Flash, 8 MB PSRAM).
  Firmware bootet erfolgreich, arbeitet normal (ETH UP, UDP OK, Modul-Detection OK), aber
  nach ~30 Sekunden tritt ein Task-Watchdog-Reset auf:

  ```
  E (30277) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
  E (30277) task_wdt:  - IDLE0 (CPU 0)
  E (30277) task_wdt: Tasks currently running:
  E (30277) task_wdt: CPU 0: maint
  E (30277) task_wdt: CPU 1: IDLE1
  ```

  Hinweis: Der WDT-Report nennt `CPU 0: maint`, aber der maintTask wird in diesem Build
  **nicht** erstellt (`feat::act()` ist false auf ESP32-Classic weil ACT-Pins nicht verdrahtet sind).
  Der tatsächliche Blocker ist der `comm` Task auf Core 0.

- **Diskussion**:
  Direkt: https://chat.z.ai/c/32dd8e42-f683-4294-b86d-e30515b36891

- **Kontext/Problem**:

  Der Aufgabenmodus ist gemischt: Die Validierung ist hardware_required, die Implementierung firmware_only.

  Der FreeRTOS Task Watchdog überwacht, dass die IDLE-Tasks auf beiden CPUs regelmäßig
  laufen kommen. Der Reset tritt auf weil der IDLE-Task auf CPU 0 verhungert — ein Task
  auf Core 0 blockiert die CPU lange genug, um den WDT auszulösen (Default-Timeout: 5 s,
  aber der Trigger erfolgt nach ~30 s, was auf einen inkonsistenten Timeout-Wert hindeutet).

  **Verdächtige Faktoren:**

  1. **ESP32 Classic RMII Ethernet-Treiber**: Der ESP32 Classic nutzt internen EMAC mit
     RMII-PHY (RTL8201). Der ETH-Treiber nutzt DMA auf Core 0 und kann unter Netzwerklast
     IRQ-Verarbeitung prioritär behandeln, die den IDLE-Task verdrängt. Dies ist ein
     bekanntes Verhalten des ESP32 Classic (nicht ESP32-S3, der einen separaten Ethernet-MAC
     hat).

  2. **commTask auf Core 0 (100 Hz Poll-Loop)**: Läuft mit Priority `configMAX_PRIORITIES - 3`.
     Enthält `netPollReceive()` (UDP), `ntripTick()`, `ntripReadRtcm()`, `netSendAogFrames()`.
     NTRIP ist `state=ON` mit leerem Host (bleibt in IDLE). `ntripTick()` enthält aber
     `hal_tcp_connect()` mit 5 s Timeout im `CONNECTING` State — sollte nicht erreicht
     werden wenn host="", aber der Pfad existiert im commTask-Kontext.

  3. **Halte-Problem beim WDT-Tasknamen**: Der WDT meldet `"CPU 0: maint"`, obwohl der
     maintTask nicht erstellt wurde. Der Name könnte von einem vorherigen Flash stammen
     oder der FreeRTOS-Task-Name-Cache ist inkonsistent. Dies erschwert die Diagnose
     und muss geklärt werden.

  4. **`CONFIG_ESP_TASK_WDT_TIMEOUT_S`**: Der Default-Wert im Arduino ESP32 Core ist
     typischerweise 5 s. Der Trigger nach ~30 s deutet darauf hin, dass der Timeout
     entweder höher konfiguriert ist oder der WDT mehrfach resettet wird bevor er
     auslöst. Die sdkconfig-Einstellung muss verifiziert werden.

  **Bekannte Einschränkungen aus der Diagnose:**
  - Base-Env `T-ETH-Lite-ESP32` (nur `-DFEAT_ETH`) hat den gleichen ETH-Treiber
    und denselben commTask. Ob dort der gleiche WDT-Reset auftritt, ist **nicht** getestet.
  - Der Reset ist reproduzierbar aufgetreten (2× hintereinander im selben Chat).
  - Diagnose-Logs (IMU/WAS/SPI) laufen auf Core 1 weiter bis zum Reset — beweist
    dass Core 1 nicht betroffen ist.

- **Scope (in)**:

  ### Analyseteil (Phase 1)
  - **Ursachenanalyse**: Systematisch klären warum der IDLE-Task auf Core 0 verhungert:
    a) ETH-DMA/IRQ-Last auf Core 0 messen (z.B. `esp_task_wdt_reset()` Call-Frequenz
       in `loop()` loggen, `uxTaskGetStackHighWaterMark()` für alle Tasks loggen).
    b) Klarstellen ob der WDT-Report `"CPU 0: maint"` irreführend ist oder ob tatsächlich
       ein maintTask existiert (Task-Liste via `vTaskList()` ausgeben).
    c) Prüfen ob der Reset auch mit Base-Env `T-ETH-Lite-ESP32` (nur `-DFEAT_ETH`,
       kein NTRIP, kein GNSS) auftritt → isoliert ob es ein ETH-Treiber-Problem oder
       ein Feature-spezifisches Problem ist.
    d) Prüfen ob Netzwerk-Traffic (AgIO verbunden oder nicht) den Auslöser beeinflusst.
  - **Config-Verifikation**: Aktuellen `CONFIG_ESP_TASK_WDT_TIMEOUT_S` Wert aus dem
    generierten `sdkconfig` lesen und mit dem beobachteten ~30 s Trigger vergleichen.

  ### Fix-Teil (Phase 2)
  Basierend auf den Analyseergebnissen:
  - **Wenn ETH-Treiber-Problem**: Watchdog-Timeout erhöhen oder IDLE-Task-Feeding
    verbessern. Mögliche Maßnahmen:
    a) `CONFIG_ESP_TASK_WDT_TIMEOUT_S` in `sdkconfig.defaults` erhöhen (z.B. 15 s).
    b) `CONFIG_ESP_TASK_WDT_PANIC` deaktivieren (nur Reset, kein Panic/Abort).
    c) `esp_task_wdt_reset()` zusätzlich im commTask aufrufen.
  - **Wenn NTRIP/Feature-spezifisch**: `ntripTick()` Blockierungs-Pfad identifizieren
    und entfernen. Laut ADR-GNSS-001 und ADR-002 darf `ntripTick()` im commTask
    nicht blockieren (connect gehört in maintTask — aktuell aber nur commTask läuft
    wenn maintTask nicht erstellt wurde).
  - **Wenn Task-Name-Cache-Problem**: Dokumentieren, ggf. Task-Namen explizit setzen.
  - **sdkconfig.defaults anpassen**: WDT-Konfiguration für ESP32 Classic optimieren.

- **Nicht-Scope (out)**:
  - Externe Hardware-Watchdog-Integration (→ TASK-005).
  - ETH-Treiber-Code ändern (ESP-IDF intern, out of scope).
  - MaintTask conditional erstellen auch wenn `feat::act()` false ist
    (→ separater Task, falls gewünscht).
  - PSRAM-Konfiguration ändern.

- **Pflichtlektüre vor Umsetzung**:
  1. `README.md`
  2. `agents.md`
  3. `docs/adr/ADR-002-task-model-control-comm-maint.md` (Taskmodell-Invarianten)
  4. `docs/adr/ADR-GNSS-001-ntrip-single-base-policy.md` (NTRIP im maintTask)
  5. `docs/adr/ADR-LOG-001-logging-buffering-and-sd-flush-policy.md` (maintTask-Priorität)
  6. `backlog/tasks/TASK-029-maintask-fuer-blocking-ops-und-psram-sd-logging.md` (maintTask-Design)
  7. `backlog/tasks/TASK-005-externer-hardware-watchdog.md` (Hardware-Watchdog, später)
  8. `sdkconfig.defaults` (aktuelle WDT-Einstellungen)
  9. `src/main.cpp` (commTask-Loop, loop(), Task-Erstellung)
  10. `src/hal_esp32/sd_logger_esp32.cpp` (maintTask-Erstellung, conditional)
  11. `src/logic/ntrip.cpp` (ntripTick(), blocking Paths)
  12. `src/hal_esp32/hal_impl.cpp` (hal_tcp_connect(), ETH-Init)
  13. `platformio.ini` (Env-Konfiguration, Profile)
  14. dieser Task (TASK-045)

- **files_read**:
  - `sdkconfig.defaults`
  - `sdkconfig_s3.defaults`
  - `src/main.cpp` (Task-Erstellung, commTask, loop(), WDT-Reset)
  - `src/hal_esp32/sd_logger_esp32.cpp` (maintTask-Erstellung, sdLoggerMaintInit)
  - `src/logic/ntrip.cpp` (ntripTick Blocking-Pfade, State-Machine)
  - `src/logic/ntrip.h` (API)
  - `src/logic/net.cpp` (netPollReceive, netSendAogFrames)
  - `src/hal_esp32/hal_impl.cpp` (hal_tcp_connect, ETH-Init, hal_delay_ms)
  - `src/logic/modules.cpp` (moduleIsActive, compiled-Flag-Logik)
  - `platformio.ini` (Env-Konfiguration, Build-Flags)
  - `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h` (Pin-Counts)

- **files_write**:
  - `sdkconfig.defaults` (WDT-Konfiguration, falls nötig)
  - `src/main.cpp` (diagnostische Ausgabe, ggf. WDT-Reset im commTask)
  - `src/hal_esp32/sd_logger_esp32.cpp` (falls maintTask-Erstellungslogik geändert wird)
  - `src/logic/ntrip.cpp` (falls Blocking-Pfad im commTask korrigiert wird)
  - `reports/TASK-045/` (Entwickler-Report mit Analyseergebnissen)

- **public_surface**:
  - `sdkconfig.defaults` (WDT-Einstellungen)
  - `src/main.cpp` (Task-Struktur)

- **merge_risk_files**:
  - `sdkconfig.defaults` (Änderungen an WDT-Config betreffen alle ESP32-Builds)
  - `src/main.cpp` (Task-Struktur, commTask-Loop)
  - `src/logic/ntrip.cpp` (State-Machine, Blocking-Pfade)

- **risk_notes**:
  - **ETH-Treiber-Limitation**: Wenn das Problem im ESP32 EMAC-Driver liegt, kann es
    nicht im Firmware-Code gelöst werden, sondern nur durch WDT-Toleranz-Anpassung.
    Das ist ein known limitation des ESP32 Classic (nicht ESP32-S3).
  - **maintTask nicht erstellt**: Aktuell wird maintTask nur erstellt wenn
    `feat::act() && feat::safety() && moduleIsActive(MOD_SD)` (main.cpp:567).
    Auf dem ESP32-Classic-Board ist `feat::act() = false` (keine ACT-Pins).
    Dadurch läuft `ntripTick()` im commTask statt im maintTask — ein Verstoß gegen
    ADR-002 Invariante "commTask darf keine blockierende Connect-Logik ausführen".
  - **False-Positive WDT**: Wenn der IDLE-Task nur gelegentlich (nicht deterministisch)
    verhungert, kann eine reine Timeout-Erhöhung das Problem nur verschieben, nicht lösen.
    Die Ursache muss identifiziert werden.

- **Invarianten**:
  - ADR-002: `commTask` darf keine blockierende Connect-Logik ausführen. `ntripTick()`
    mit `hal_tcp_connect()` (5 s Timeout) gehört in `maintTask`, nicht in `commTask`.
  - ADR-002: `maintTask` darf blockieren, solange die höheren Pfade stabil bleiben.
  - ADR-GNSS-001: NTRIP Connect-/Reconnect-Pfad gehört in `maintTask`.
  - ADR-LOG-001: `controlTask` darf keine direkten SD-Schreiboperationen ausführen.
  - FreeRTOS IDLE-Tasks auf beiden CPUs müssen regelmäßig laufen (WDT-Invariante).

- **Known traps**:
  - **WDT-Taskname-Cache**: Der FreeRTOS WDT meldet den Tasknamen zum Zeitpunkt der
    Registrierung, nicht zum Zeitpunkt des Triggers. Ein Task der gelöscht wurde
    (oder nie erstellt) kann trotzdem im WDT-Report erscheinen wenn der Name-Cache
    nicht aktualisiert wurde.
  - **ESP32 vs ESP32-S3 ETH**: Der ESP32 Classic nutzt EMAC (internal MAC + external
    RMII PHY). Der ESP32-S3 hat keinen internen MAC — Ethernet läuft über W5500 (SPI).
    ETH-Verhalten ist zwischen den Boards **nicht** vergleichbar. Fixes für den Classic
    dürfen den S3-Pfad nicht beeinträchtigen.
  - **`CONFIG_ESP_TASK_WDT_TIMEOUT_S` wirkt nur nach Clean-Build**: Änderungen in
    `sdkconfig.defaults` werden nur beim ersten Build übernommen (generiertes
    `sdkconfig` muss gelöscht werden → `.pio` Verzeichnis löschen).
  - **MaintTask conditional**: Wenn maintTask nicht erstellt wird (weil feat::act() false),
    MUSS ntripTick() trotzdem irgendwo laufen. Aktuell im commTask — das ist der
    Konflikt mit ADR-002. Die Lösung muss beide Pfade abdecken.

- **Rejected alternatives**:
  - `CONFIG_ESP_TASK_WDT_PANIC=n` (nur Reset statt Panic/Abort):
    verworfen, da der Panic-Stack-Trace wertvolle Diagnose-Informationen liefert.
    Besser: Panic behalten, Timeout erhöhen, Ursache beheben.
  - NTRIP komplett aus commTask entfernen wenn maintTask nicht läuft:
    verworfen, da NTRIP dann gar nicht mehr funktioniert (kein connect, kein reconnect).
    Besser: maintTask bedingungslos erstellen oder ntripTick() nicht-blockierend halten.
  - ETH-Treiber durch Arduino Ethernet Library ersetzen:
    verworfen, da der ESP-IDF ETH-Treiber korrekt ist und der Wechsel Breaking Changes
    und neue Dependencies einführt.

- **AC**:
  - **Analyse**: Ursache des WDT-Resets ist identifiziert und dokumentiert (Entwickler-Report).
  - **Reproduktion**: Reset ist reproduzierbar mit `profile_full_steer` auf ESP32 Classic.
  - **Basis-Vergleich**: Getestet ob Reset auch mit Base-Env `T-ETH-Lite-ESP32` auftritt.
  - **Task-Namen**: Klar dokumentiert warum WDT `"CPU 0: maint"` meldet.
  - **WDT-Konfiguration**: `CONFIG_ESP_TASK_WDT_TIMEOUT_S` ist explizit in
    `sdkconfig.defaults` gesetzt (Wert basierend auf Analyse).
  - **ntripTick()**: Läuft nicht mehr im commTask wenn maintTask nicht erstellt wurde,
    ODER ist garantiert nicht-blockierend im commTask-Kontext.
  - **Kein Regression**: ESP32-S3 Builds sind nicht betroffen (ETH läuft über SPI/W5500).
  - **Stability-Test**: Firmware läuft mindestens 5 Minuten ohne WDT-Reset bei
    verbundenem Ethernet und aktiver AgIO-Kommunikation.
  - `pio run -e profile_full_steer` baut fehlerfrei.
  - `pio run -e T-ETH-Lite-ESP32` baut fehlerfrei.

- **verification**:
  ```bash
  # 1. Build beide ESP32-Envs
  pio run -e profile_full_steer
  pio run -e T-ETH-Lite-ESP32

  # 2. Flash + Serial Monitor (mindestens 5 Minuten)
  pio run -e profile_full_steer -t upload -t monitor

  # 3. Basis-Vergleich (mindestens 5 Minuten)
  pio run -e T-ETH-Lite-ESP32 -t upload -t monitor

  # 4. Entwickler-Report vorhanden
  ls reports/TASK-045/
  ```

- **Links**:
  - `backlog/epics/EPIC-001-runtime-stability.md`
  - `docs/adr/ADR-002-task-model-control-comm-maint.md`
  - `docs/adr/ADR-GNSS-001-ntrip-single-base-policy.md`
  - `docs/adr/ADR-LOG-001-logging-buffering-and-sd-flush-policy.md`
  - `backlog/tasks/TASK-029-maintask-fuer-blocking-ops-und-psram-sd-logging.md`
  - `backlog/tasks/TASK-005-externer-hardware-watchdog.md`
