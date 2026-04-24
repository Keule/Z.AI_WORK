# Prompt: Komplettes Modulsystem-Refactoring in einem Aufwasch

Du arbeitest im ESP32-Firmware-Projekt unter `/home/z/my-project/esp32_project/`.
Dies ist ein PlatformIO-Projekt (ESP32-S3, LilyGO T-ETH-Lite-S3).

## Kontext

Die Firmware hat aktuell ein fragmentiertes Modulsystem. Es existiert ADR-MODULE-001
(module_interface.h), aber die meisten Module halten es nicht ein. Ziel ist ein
vollständiges Refactoring in EINEM Durchlauf.

## Was zu tun ist

### Schritt 1: ADR schreiben

Schreibe `/home/z/my-project/esp32_project/docs/adr/ADR-MODULE-002-unified-module-system.md`
mit folgendem Inhalt (vervollständige und formalisiere):

- Status: accepted
- Datum: heute
- Motivation: Aktuelle Module sind heterogen, ADR-MODULE-001 wird nicht eingehalten,
  CLI ist modulspezifisch, Resource-Management bei Deaktivierung fehlt, zwei Modi
  (config/work) statt drei.
- Entscheidung:

  **Modi:** Nur noch `config` und `work`. Kein `paused` mehr.
  - `config`: System konfigurierbar, Control-Loop gestoppt, Netzwerk aktiv, CLI voll.
  - `work`: Autosteer aktiv, Control-Loop 200Hz, Netzwerk aktiv.
  - Boot ist transient und geht automatisch nach init in `config`.

  **Modulliste (endgültig):**
  ```cpp
  enum class ModuleId : uint8_t {
      ETH = 0,        // Transport: W5500 Ethernet
      WIFI,           // Transport: WiFi (AP/STA)
      BT,             // Transport: Bluetooth SPP
      NETWORK,        // Protokoll: PGN Codec RX/TX
      GNSS,           // Sensor: UM980 UART
      NTRIP,          // Service: RTCM Client
      IMU,            // Sensor: BNO085 SPI
      WAS,            // Sensor: ADS1118 / Wheel Angle Sensor
      ACTUATOR,       // Aktor: DRV8263 SPI
      SAFETY,         // Sensor: Safety-Loop GPIO + Watchdog
      CONTROL,        // Logik: PID Controller
      LOGGING,        // Service: SD-Card Logger
      OTA,            // Service: OTA Update
      COUNT
  };
  ```
  Gestrichene "Module": logsw (wird Konfig-Parameter von logging), sd (Geräteknoten,
  kein eigenes Modul).

  **Abhängigkeitsgraph:**
  ```
  NETWORK benötigt: ETH oder WIFI oder BT (mindestens einer)
  NTRIP benötigt: NETWORK, GNSS
  CONTROL benötigt: IMU, WAS, ACTUATOR, SAFETY
  LOGGING benötigt: (keine zwingend, aber ohne SD nutzlos)
  OTA benötigt: ETH oder WIFI oder BT
  ```

  **Einheitliche Modulschnittstelle:**
  Jedes Modul MUSS folgende Funktionen implementieren (alle existieren immer,
  auch als No-Op wenn das Modul keinen Beitrag liefert):

  ```cpp
  // Lifecycle
  bool mod_<name>_is_enabled(void);                              // Compile-time Feature-Check
  void mod_<name>_activate(void);                                // Pins claimen, Hardware init, Tasks starten
  void mod_<name>_deactivate(void);                              // Alles freigeben (Pins, Timer, Tasks, UART, Speicher)
  bool mod_<name>_is_healthy(uint32_t now_ms);                   // Health-Check (4 Bedingungen)

  // Pipeline (nur in "work" aufgerufen)
  ModuleResult mod_<name>_input(uint32_t now_ms);                // Sensoren lesen, Daten empfangen
  ModuleResult mod_<name>_process(uint32_t now_ms);              // Daten verarbeiten
  ModuleResult mod_<name>_output(uint32_t now_ms);               // Aktoren ansteuern, Daten senden

  // Konfiguration
  bool mod_<name>_cfg_get(const char* key, char* buf, size_t len); // Konfigwert lesen
  bool mod_<name>_cfg_set(const char* key, const char* val);       // Konfigwert setzen (RAM only)
  bool mod_<name>_cfg_apply(void);                                  // Konfig aktivieren (re-init)
  bool mod_<name>_cfg_save(void);                                   // Konfig nach NVS schreiben
  bool mod_<name>_cfg_load(void);                                   // Konfig aus NVS laden
  bool mod_<name>_cfg_show(void);                                   // Konfig über CLI ausgeben
  bool mod_<name>_debug(void);                                      // Lifecycle-Test durchlaufen

  // ModuleOps2 Registry (existing, updated)
  struct ModuleOps2 {
      const char*    name;
      ModuleId       id;
      bool           (*is_enabled)(void);
      void           (*activate)(void);
      void           (*deactivate)(void);
      bool           (*is_healthy)(uint32_t now_ms);
      ModuleResult   (*input)(uint32_t now_ms);
      ModuleResult   (*process)(uint32_t now_ms);
      ModuleResult   (*output)(uint32_t now_ms);
      bool           (*cfg_get)(const char* key, char* buf, size_t len);
      bool           (*cfg_set)(const char* key, const char* val);
      bool           (*cfg_apply)(void);
      bool           (*cfg_save)(void);
      bool           (*cfg_load)(void);
      bool           (*cfg_show)(void);
      bool           (*debug)(void);
      const ModuleId* deps;        // Array, nullptr-terminiert
      const char*     dep_reasons; // Menschlesbare Begründung je Dep
  };
  ```

  **ModState (aus ADR-MODULE-001, beibehalten):**
  ```cpp
  struct ModState {
      bool     detected;       // Hardware/Abhängigkeit erkannt
      bool     quality_ok;     // Daten-/Funktionsqualität innerhalb Grenzwerte
      uint32_t last_update_ms; // Zeitstempel letzte erfolgreiche Aktualisierung
      int32_t  error_code;     // 0 = OK, !=0 modulspezifisch
  };
  ```
  Jedes Modul MUSS intern einen ModState führen. is_healthy() MUSS alle 4
  Bedingungen prüfen (detected, quality_ok, freshness, error_code==0).

  **ModuleResult:**
  ```cpp
  struct ModuleResult {
      bool     success;
      uint32_t error_code;  // 0 = OK
  };
  ```

  **Resource-Management bei Deaktivierung:**
  deactivate() MUSS: GPIO-Pins freigeben (hal_pin_claim_release), Timer/Interrupts
  stoppen, dynamischen Speicher freigeben, FreeRTOS-Tasks löschen, UART flush+close.

  **CLI (einheitlich für alle Module):**
  ```
  module <name> show              → Zustand + Health + Konfig
  module <name> set <key> <val>   → Konfigwert setzen (RAM)
  module <name> load              → aus NVS laden
  module <name> apply             → Konfig aktivieren
  module <name> save              → nach NVS schreiben
  module <name> activate          → aktivieren (Dep-Check + Pin-Claim)
  module <name> deactivate        → deaktivieren (Ressourcen freigeben)
  module <name> debug             → Lifecycle-Check
  module list                     → alle Module + Status
  mode config | work              → Modus wechseln
  ```
  Die CLI-Befehle werden EINMAL generisch implementiert und rufen über ModuleOps2
  die modulspezifischen Funktionen auf. KEINE modulspezifischen CLI-Handler mehr.

  **No-Op-Konvention:**
  Wenn ein Modul keinen Beitrag zu einer Pipeline-Phase liefert, implementiert es
  die Funktion trotzdem und gibt {true, 0} zurück (expliziter No-Op, nie nullptr).

  **Invarianten:**
  - In `config`-Modus werden NUR activate/deactivate/cfg-*/debug aufgerufen.
    input/process/output werden NICHT aufgerufen.
  - In `work`-Modus werden input/process/output zyklisch aufgerufen.
    activate/deactivate sind in work erlaubt aber sollten mit Warnung quittiert werden.
  - is_healthy() darf keine Seiteneffekte haben.
  - error_code muss je Modul dokumentiert und stabil sein.
  - Abhängigkeitsprüfungen bei activate(): Wenn eine Dep fehlt → Fehlermeldung
    mit Angabe welches Modul fehlt und warum.

  **DoD-Kriterien (Phase 4):**
  1. API-Vollständigkeit: Alle 13 Module haben alle 15+1 Funktionen
  2. State-Vollständigkeit: Alle Module haben ModState mit 4 Feldern
  3. Health-Konsistenz: is_healthy() prüft alle 4 Bedingungen bei allen 13 Modulen
  4. Freshness-Verhalten: is_healthy() wird false bei Timeout
  5. Resource-Management: deactivate() gibt alle Ressourcen frei
  6. CLI: Generische Implementierung, keine modulspezifischen Handler
  7. NVS: cfg_save/cfg_load funktioniert für alle Module

  **ADR-MODULE-001 bleibt gültig** für die 4 Pflichtsignaturen und den ModState.
  ADR-MODULE-002 erweitert und superseded ADR-MODULE-001.

---

### Schritt 2: Implementierung

Baue ALLES in einem Durchlauf. Die Reihenfolge:

1. **Dateien anlegen/umbenennen:**
   - `src/logic/module_interface.h` → NEUE Datei mit ModuleId-Enum, ModuleResult,
     ModState, ModuleOps2, und dem zentralen Module-Registry-Array
   - `src/logic/module_system.h` / `module_system.cpp` → Zentrale Funktionen:
     module_init_all(), module_activate(ModuleId), module_deactivate(ModuleId),
     module_is_healthy(ModuleId, uint32_t), module_get_state(ModuleId),
     module_run_pipeline_input(uint32_t), module_run_pipeline_process(uint32_t),
     module_run_pipeline_output(uint32_t), module_list(), mode_get(), mode_set()
   - Für JEDES der 13 Module:
     `src/logic/mod_<name>.h` und `src/logic/mod_<name>.cpp`

2. **Jedes Modul implementiert:**
   - Alle 15 Funktionen aus ModuleOps2
   - Internen ModState
   - Abhängigkeiten als const Array
   - Konfiguration (set/get/save/load/apply/show)
   - Debug-Lifecycle: config→init→input→process→output→deinit mit Logging

3. **Vorhandenen Code migrieren:**
   - `imu.h/.cpp` → `mod_imu.h/.cpp` (Logik übernehmen, Signatur anpassen)
   - `was.h/.cpp` → `mod_was.h/.cpp`
   - `actuator.h/.cpp` → `mod_actuator.h/.cpp`
   - `control.h/.cpp` → `mod_control.h/.cpp`
   - `net.h/.cpp` → aufteilen in `mod_eth.h/.cpp` (Transport) und
     `mod_network.h/.cpp` (PGN-Protokoll)
   - `sd_logger.h/.cpp` → `mod_logging.h/.cpp`
   - `ntrip.h/.cpp` → `mod_ntrip.h/.cpp`
   - Neue Module: `mod_wifi.h/.cpp`, `mod_bt.h/.cpp`, `mod_gnss.h/.cpp`,
     `mod_safety.h/.cpp`, `mod_ota.h/.cpp`

4. **CLI umbauen:**
   - `cli.cpp` → generische `cmd_module()` Funktion die über ModuleOps2-Registry
     iteriert und die modulspezifischen cfg/debug-Funktionen aufruft
   - Bestehende cmd_*.cpp Dateien die modulspezifische CLI-Handler enthalten
     werden durch das generische System ersetzt
   - `cmd_config.cpp` anpassen: `save` ruft cfg_save() aller aktiven Module auf

5. **main.cpp anpassen:**
   - OpMode: `config` und `work` statt BOOTING/ACTIVE/PAUSED
   - In `work`: Pipeline-Loop über alle aktiven Module (input→process→output)
   - In `config`: nur net_poll für CLI-Kommunikation, kein Control-Loop

6. **NVS-Konfiguration:**
   - Namespace pro Modul: `mod_<name>` (z.B. `mod_eth`, `mod_imu`)
   - cfg_save/cfg_load nutzen bestehende nvs_config Infrastruktur

7. **op_mode.h/.cpp anpassen:**
   - Enum: `enum class OpMode { CONFIG, WORK };`
   - mode_set(WORK) prüft ob alle benötigten Module aktiv und healthy sind
   - mode_set(CONFIG) stoppt Pipeline-Loop

8. **HAL-Schicht:**
   - Nicht verändern! HAL bleibt wie sie ist. Modul-Schicht ruft HAL auf.
   - Neue HAL-Funktionen für WiFi/BT wenn nötig (hal_wifi_*, hal_bt_*)

9. **features.h:**
   - BLEIBT für Compile-Time-Feature-Gates. Wird von mod_*_is_enabled() genutzt.

---

### Schritt 3: Bestehende Dateien bereinigen

- Alte Module-Dateien (imu.h, was.h, actuator.h, control.h, net.h, sd_logger.h,
  ntrip.h, etc.) die durch mod_*.h ersetzt wurden: LÖSCHEN
- Alte cmd_*.cpp die jetzt vom generischen CLI-System abgedeckt werden: LÖSCHEN
- modules.h / modules.cpp: Ersetzen durch module_system.h / module_system.cpp
- module_interface.h: Komplett neu schreiben
- op_mode.h / op_mode.cpp: Anpassen auf CONFIG/WORK

---

## Wichtige Regeln

- BAUE DIREKT, ohne Rückfrage. Wenn du eine Entwurfsentscheidung treffen musst,
  triff sie pragmatisch.
- HAL-Schicht NICHT verändern (`src/hal/` und `src/hal_esp32/`).
- Der Build MUSS am Ende kompilieren (zumindest syntaktisch korrekt,
  semantische Fehler in der Migration sind tolerierbar).
- Schreibe nach Fertigstellung einen zusammenfassenden Eintrag ins worklog:
  `/home/z/my-project/worklog.md`
- Der bestehende ADR-MODULE-001-Text bleibt bestehen, ADR-MODULE-002 referenziert ihn.
- NVS-Key-Namespace: `mod_<name>` (z.B. `mod_eth_baud`, `mod_imu_rate`)
- Alle neuen .cpp/.h Dateien bekommen einen File-Header mit @brief und @file.
- Logging: Nutze bestehendes LOGI/LOGW/LOGE Pattern mit Modul-Tag.
