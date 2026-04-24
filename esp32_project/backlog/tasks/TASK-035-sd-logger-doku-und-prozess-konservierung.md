# Task: sd_logger_esp32.cpp Dokumentation und Prozess §3 Chat-Konservierung (Review-F7 + Prozess)

- **Origin:** Kombinierter Review TASK-026..030, Finding F7 (Niedrig), agents.md §3 (Prozess)
- **Entscheidung:** Planer-Entscheidung (klar, keine Ambiguität)

## Kontext / Problem

### F7: sd_logger_esp32.cpp Dokumentation unvollständig

Die PSRAM-Buffer-Dokumentation in `sd_logger_esp32.cpp` wurde im 5-Mängel-Fix verbessert ("buffer selection happens ONCE at init", "SD_SPI_BUS bus is shared... blocks sensor SPI for ~50-200 ms"). Es fehlt jedoch ein Hinweis darauf, dass auf dem **ESP32 Classic** keine Sensor-SPI-Umschaltung nötig ist, da dort keine separaten Sensor-Pins vorhanden sind (alle Sensor-Pins sind -1).

### Prozess §3: Chat-Inhalte konservieren

Per agents.md §3: *"Wichtige Chatinhalte müssen in Task, ADR oder Report materialisiert werden."* Die Diskussion über GPIO-46-Konflikt, Profil-Naming und NTRIP-Credentials existierte bisher nur im Chat. Mit den TASK-031..034 wird diese Diskussion formal in Backlog-Tasks überführt. Zusätzlich müssen folgende Punkte konserviert werden:

1. Die Entscheidung für Variante (a) bei Legacy Pin-Claims (TASK-031)
2. Die Entscheidung für dateibasiertes Credential-Laden (TASK-033)
3. Die GPIO-46 Konfliktentscheidung (TASK-034)

## Akzeptanzkriterien

### F7:
1. ~~In `sd_logger_esp32.cpp` ein Kommentar ergänzen...~~ **ERLEDIGT** durch KI-Planer (siehe Commit-Nachricht). Kommentar in Zeilen 29-34 der Datei:
   ```cpp
   // ESP32 Classic (T-ETH-Lite-ESP32):
   //   No sensor SPI bus switching is required for SD access.  Although the SD
   //   card and the sensor bus share the same HSPI peripheral, all sensor pins
   //   (IMU, ADS, ACT) are set to -1 in the Classic board profile — no sensor
   //   peripherals are initialised, so the SD card has exclusive use of HSPI.
   //   The SPI re-init dance (deinit/reinit) is effectively a no-op on Classic.
   ```
2. ~~Der Kommentar ist präzise und irreführt nicht...~~ **ERLEDIGT** — Kommentar korrekt: SD und Sensor teilen sich auf dem Classic denselben HSPI-Bus, aber wegen Pins=-1 gibt es keinen Konflikt.

### Prozess §3 (Vollständigkeitsprüfung durch KI-Planer):
3. TASK-031 dokumentiert die Entscheidung für Variante (a) im Abschnitt "Entscheidung Mensch" — **OK**
4. TASK-033 dokumentiert die Credentials-Datei-Entscheidung — **OK**
5. TASK-034 dokumentiert die GPIO-3-Verlegungs-Entscheidung — **OK**
6. Alle drei Tasks sind im `backlog/index.yaml` eingetragen — **OK**
7. Review-Reports unter `reports/TASK-026-030-review/` konserviert — **OK** (review.md, review.pdf, gpt-5.3-codex.md)

## Scope (in)

- `src/hal_esp32/sd_logger_esp32.cpp`: Einen Kommentar ergänzen
- `backlog/tasks/TASK-031*.md`: Existiert bereits mit Entscheidungs-Doku
- `backlog/tasks/TASK-033*.md`: Existiert bereits mit Entscheidungs-Doku
- `backlog/tasks/TASK-034*.md`: Existiert bereits mit Entscheidungs-Doku
- `backlog/index.yaml`: TASK-031..034 eingetragen

## Nicht-Scope (out)

- Keine Änderung am Funktionscode
- Kein neuer ADR erforderlich (Entscheidungen sind in den Tasks dokumentiert)

## Verifikation / Test

- `pio run -e T-ETH-Lite-ESP32S3` — muss kompilieren
- `pio run -e T-ETH-Lite-ESP32` — muss kompilieren
- `rg "ESP32 Classic" src/hal_esp32/sd_logger_esp32.cpp` — zeigt den neuen Kommentar

## Relevante ADRs

- **ADR-LOG-001** (Logging-Pufferung): Konform, kein ADR-Update nötig. Die Dokumentation ergänzt nur eine hardware-spezifische Besonderheit.
- **agents.md §3:** Chat-Inhalte werden in Repo-Artefakte überführt (diese Tasks).

## Invarianten

- Der Kommentar ändert kein Laufzeitverhalten
- ESP32 Classic SPI-Bus: SD_SPI_BUS und SENS_SPI_BUS sind beide HSPI (geteilter Bus). Da alle Sensor-Pins -1 sind, hat SD exklusiven Zugang — kein Umschalten nötig.

## Known Traps

1. **ESP32 Classic SPI-Bus-Namen:** Der Classic nutzt `HSPI` für SD (siehe `LILYGO_T_ETH_LITE_ESP32_board_pins.h` Zeile 63: `#define SD_SPI_BUS HSPI`). Der Kommentar sollte diesen konkreten Namen erwähnen, um korrekt zu sein.

## Merge Risk

- **Keines:** Nur ein Kommentar wird ergänzt.

## Classification

- **category:** platform_reuse
- **priority:** low
- **delivery_mode:** firmware_only
- **exclusive_before:** Keine
- **parallelizable_after:** Parallel mit allen anderen TASK-03x

## Owner

- **Assigned:** KI-Planer
- **Status:** done
- **Ausgeführt von:** KI-Planer (2026-04-21)
