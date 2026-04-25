# TASK-029 Maintenance-Task fuer blocking Ops und PSRAM-SD-Logging

- **ID**: TASK-029
- **Titel**: `maintTask` fuer blocking Operationen einfuehren, SD-Logging auf PSRAM-Buffer umstellen
- **Status**: done
- **Priorität**: high
- **Komponenten**: src/main.cpp, src/hal_esp32/sd_logger_esp32.cpp, src/logic/ntrip.cpp, src/logic/ntrip.h, src/logic/sd_logger.cpp, src/logic/sd_logger.h, src/logic/control.cpp
- **Dependencies**: TASK-027
- **delivery_mode**: firmware_only
- **task_category**: runtime_stability
- **Owner**: ki-planer
- **Epic**: EPIC-001

- **classification**: dependent
- **exclusive_before**: [TASK-027]
- **parallelizable_after**: []

- **Origin**:
  Nutzer-Analyse: "Ich sehe 4 Problemfaelle: 1. SD-Karte beschreiben, 2. NTRIP connect, 3. IMU-Kalibration, 4. Ethernet-Verbindung." Loesung: "Alle 4 nicht in zeitkritischen Tasks ausfuehren. Einen dritten Task mit geringerer Prioritaet (unterbrechbar). Daten erst in PSRAM loggen und nur waehrend des dritten Tasks auf SD syncen. NTRIP connection und IMU-Kalibration nur wenn Nutzer das Fahrzeug anhaelt. Ohne Ethernet geht ohnehin gar nichts mehr, muss nicht behandelt werden."

- **Diskussion**:
  - Direkt: https://chat.z.ai/c/d6f6eb9b-9217-401b-bb23-08e8c0fbca69
  - Shared: https://chat.z.ai/s/a858dd17-02e3-416c-a123-649830256a4e

- **Kontext/Problem**:
  1. **SD-Logging blockiert SD_SPI_BUS**: `loggerTask` (0.5Hz) initiiert SD_SPI_BUS-Deinit/SD-Init/Schreiben/SD_SPI_BUS-Reinit, was den Sensor-SPI-Bus 50-200ms blockiert. In dieser Zeit sind IMU, ADS1118 und Actuator auf Core 1 ohne SPI-Zugriff — tot.
  2. **NTRIP TCP-Connect blockiert commTask**: `ntripTick()` ruft `hal_tcp_connect()` auf (blocking, 5s Timeout) mitten im 100Hz commTask-Loop. Waehrend des Connects werden keine UDP-Pakete verarbeitet, kein Safety-Check.
  3. Keine Moeglichkeit, blocking Operationen (SD-Write, TCP-Connect, IMU-Kalibrierung) sauber von zeitkritischen Tasks zu trennen.
  4. IMU-Kalibrierung existiert noch nicht, soll aber vorbereitet werden.

- **Scope (in)**:
  - **`maintTask` erstellen**: Neuer FreeRTOS Task auf Core 0, Prioritaet 1 (niedrigste, unterbrechbar durch commTask und controlTask). Ersetzt den aktuellen `loggerTask`.
  - **PSRAM Ring-Buffer fuer Logging**: `controlTask` schreibt Log-Records in PSRAM-basierten Ring-Buffer (statt direkt in SD_SPI_BUS-basierten SPSC Ring-Buffer). Ziel: ~1-4 MB Buffer, ausreichend fuer Tage Logging. Schreibzugriff ~1us, kein SD_SPI_BUS-Konflikt.
  - **SD-Flush in maintTask**: maintTask liest PSRAM-Buffer und schreibt auf SD-Karte. SD_SPI_BUS wird nur vom maintTask genutzt (controlTask und commTask greifen nicht mehr auf SD zu). Da maintTask niedrigste Prioritaet hat, werden zeitkritische Tasks automatisch vorgezogen.
  - **NTRIP-Connect in maintTask**: `ntripTick()` State-Machine aufteilen:
    - `ntripConnect()` / `ntripReconnect()` → laeuft in maintTask (blocking OK).
    - `ntripReadRtcm()` / `ntripForwardRtcm()` → bleibt in commTask (nur non-blocking).
    - commTask prueft NTRIP-Status, verarbeitet aber nur wenn State == CONNECTED.
  - **ETH-Monitor in maintTask**: ETH-Verbindungsstatus ueberwachen, Console-Ausgabe bei Verlust.
  - **Vorbereitung IMU-Kalibrierung**: Placeholder/Hook fuer zukuenftige IMU-Kalibrierung in maintTask (keine Implementierung).
  - **Nutzer-Meldungen**: Bei NTRIP-Verlust: Console-Meldung "Fahrzeug anhalten, dann 'ntrip reconnect' senden". Kein automatischer Reconnect waehrend der Fahrt.

- **Nicht-Scope (out)**:
  - IMU-Kalibrierung implementieren (nur Hook/Placeholder).
  - Serial-Command-Parser fuer "ntrip reconnect" (spaeterer Task).
  - WebUI fuer Status/Meldungen.

- **files_read**:
  - `src/main.cpp` (Task-Erstellung, commTaskFunc, controlTaskFunc)
  - `src/hal_esp32/sd_logger_esp32.cpp` (aktueller loggerTask, SD_SPI_BUS-Handling)
  - `src/logic/sd_logger.cpp` (Log-Record-Struktur, Ring-Buffer)
  - `src/logic/sd_logger.h` (API)
  - `src/logic/ntrip.cpp` (ntripTick, ntripReadRtcm, ntripForwardRtcm)
  - `src/logic/ntrip.h` (API)
  - `src/logic/global_state.h` (g_ntrip State)
  - `src/logic/control.cpp` (sdLoggerRecord Aufruf in controlTask)

- **files_write**:
  - `src/main.cpp` (maintTask-Erstellung statt loggerTask, commTask angepasst)
  - `src/hal_esp32/sd_logger_esp32.cpp` (maintTaskFunc statt loggerTaskFunc, PSRAM-Buffer)
  - `src/logic/sd_logger.cpp` (PSRAM Ring-Buffer statt SD_SPI_BUS SPSC)
  - `src/logic/sd_logger.h` (neue API: psramLogWrite, psramLogDrain)
  - `src/logic/ntrip.cpp` (ntripTick aufgeteilt: connect in maintTask, read/forward in commTask)
  - `src/logic/ntrip.h` (neue API: ntripConnect, ntripIsConnected)
  - `src/logic/control.cpp` (sdLoggerRecord → psramLogWrite)

- **public_surface**:
  - `src/logic/ntrip.h` — geaenderte API (connect/read/forward aufgeteilt)
  - `src/logic/sd_logger.h` — geaenderte API (PSRAM-basiert)

- **merge_risk_files**:
  - `src/main.cpp` — Task-Erstellung und commTask-Loop
  - `src/logic/ntrip.cpp` — NTRIP-Logik, zentrale Aenderung
  - `src/logic/control.cpp` — controlTask-Loop

- **risk_notes**:
  - PSRAM-Zugriff auf ESP32-S3R8 muss ueber `spiram` API erfolgen — pruefen ob `malloc()` automatisch PSRAM nutzt oder `heap_caps_malloc(MALLOC_CAP_SPIRAM)` noetig ist.
  - NTRIP State-Machine Aufteilung muss sicherstellen, dass commTask nicht auf CONNECTED-Daten zugreift, waehrend maintTask den State aendert. Mutex/Flag noetig.
  - maintTask-Prioritaet: Zu niedrig → SD-Flush verhungert. Zu hoch → blockiert zeitkritische Tasks. Empirische Validierung noetig.
  - ETH-Monitor im maintTask: Wenn Ethernet weg ist, kann der maintTask das ueber Console melden, aber AOG-Kommunikation ist trotzdem tot. User muss manuell neustarten.

- **AC**:
  - `maintTask` existiert, laeuft auf Core 0 mit Prioritaet 1.
  - `loggerTask` ist entfernt/ersetzt.
  - PSRAM Ring-Buffer (≥1 MB) wird in `controlTask` beschrieben (~1us pro Write).
  - `maintTask` draint PSRAM-Buffer auf SD-Karte. SD_SPI_BUS-Konflikt mit Sensor-Bus ist eliminiert.
  - NTRIP TCP-Connect laeuft in `maintTask`, nicht in `commTask`.
  - commTask ruft nur `ntripReadRtcm()` und `ntripForwardRtcm()` auf (non-blocking).
  - NTRIP-Verlust erzeugt Console-Meldung. Kein automatischer Reconnect ohne User-Interaktion.
  - ETH-Status wird ueberwacht, Verlust wird per Console gemeldet.
  - `controlTask` (200Hz) wird durch SD-Logging nicht mehr blockiert.
  - `commTask` (100Hz) wird durch NTRIP-Connect nicht mehr blockiert.
  - `pio run` baut fehlerfrei.

- **verification**:
  - `pio run` fuer alle relevanten Environments.
  - Review: Kein blocking-Aufruf in commTask (nur non-blocking).
  - Review: PSRAM-Alloc fuer Ring-Buffer verwendet `MALLOC_CAP_SPIRAM`.
  - Timing: controlTask 200Hz nicht mehr durch SD-Write unterbrochen.

- **Links**:
  - `backlog/epics/EPIC-001-runtime-stability.md`
  - `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md`
  - `backlog/tasks/TASK-027-modul-system-mit-runtime-aktivierung-und-pin-claim-arbitrierung.md`
