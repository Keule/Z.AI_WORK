# Task: NTRIP-Credentials aus soft_config.h entfernen und dateibasiertes Laden implementieren (Review-F3)

- **Origin:** Kombinierter Review TASK-026..030, Finding F3 (Kritisch)
- **Entscheidung Mensch:** Zugangsdaten sollen über eine Credentials-Datei geladen werden, nicht im Code stehen

- **Status:** done

## Kontext / Problem

`include/soft_config.h` Zeilen 18-22 enthalten echte produktive SAPOS-Caster-Zugangsdaten als compile-time Defaults:

```cpp
inline constexpr const char* NTRIP_HOST        = "euref-ip.net";
inline constexpr const char* NTRIP_MOUNTPOINT   = "KARL00DEU0";
inline constexpr const char* NTRIP_USER         = "oebhk";
inline constexpr const char* NTRIP_PASSWORD     = "0@AW!xek3ygKt3";
```

Dies stellt mehrere Probleme dar:

1. **Verstoß gegen TASK-028 AC:** TASK-028 fordert explizit *"leere Defaults für NTRIP-Zugangsdaten"*
2. **Verstoß gegen ADR-001 Invariante:** *"Beispiel-/Demo-Werte dürfen nicht stillschweigend produktive Defaults ersetzen."*
3. **Sicherheitsrisiko:** Credentials liegen als Klartext im Git-Repo / Firmware-Artefakt
4. **Betriebsrisiko:** Unbeabsichtigtes Deployment mit produktiven Credentials bei einem Demo-/Test-Build

Der Mensch hat entschieden, dass Zugangsdaten stattdessen über eine **Credentials-Datei** geladen werden sollen (SD-Karte, mit Fallback auf leere Defaults).

### Aktueller Lade-Pfad

In `main.cpp` Zeile 560-561:
```cpp
softConfigLoadDefaults(softConfigGet());      // Lädt cfg:: Defaults → RuntimeConfig
softConfigLoadOverrides(softConfigGet());     // Currently no-op stub
```

`softConfigLoadOverrides()` in `src/logic/runtime_config.cpp` ist aktuell ein **no-op stub**. Hier muss die Credentials-Datei eingelesen werden.

### Abhängigkeit von TASK-029 (maintTask)

Die SD-Karte wird im maintTask verwaltet (`sd_logger_esp32.cpp`). Das Lesen einer Credentials-Datei muss vor dem maintTask passieren (in `setup()`), damit die NTRIP-Konfiguration beim `ntripInit()`-Aufruf bereits vorhanden ist.

**Problem:** Die SD-Karte wird im maintTask erst bei Bedarf initialisiert (wenn der Logging-Switch betätigt wird). Für das Lesen der Credentials-Datei muss die SD-Karte **einmalig in `setup()`** gemounted und gelesen werden, bevor Tasks erstellt werden.

**Hinweis:** Auf dem ESP32 Classic ist die SD-Karte über einen separaten SPI-Bus erreichbar (oder über denselben mit Pin-Switching). Die genaue Hardware-Abhängigkeit muss der Entwickler prüfen.

## Akzeptanzkriterien

1. `soft_config.h`: NTRIP-Defaults auf leer setzen (`""` für Host, Mountpoint, User, Password; Port bleibt neutral)
2. `softConfigLoadOverrides()` liest eine Credentials-Datei von SD-Karte (z. B. `/ntrip.cfg`)
3. Dateiformat: Menschlesbar, z. B. INI-Style oder Key=Value:
   ```
   host=euref-ip.net
   port=2101
   mountpoint=KARL00DEU0
   user=oebhk
   password=<geheimes_passwort>
   ```
4. Fallback: Wenn Datei nicht existiert oder nicht lesbar → RuntimeConfig bleibt bei leer (NTRIP wird nicht gestartet, Log-Warnung)
5. Bestehende Override-Pfade (Serial/WebUI/NVS) bleiben unberührt (zukunftssicher)
6. `rg -l "oebhk\|0@AW\|euref-ip\|KARL00DEU0" include/` zeigt keine Treffer mehr
7. Git-History: Die alten Credentials sind aus der aktuellen Version entfernt (kein `git filter-branch` nötig, da der Mensch über Credential-Rotation entscheidet)

## Scope (in)

- `include/soft_config.h`: NTRIP-Defaults auf leer setzen
- `src/logic/runtime_config.cpp`: `softConfigLoadOverrides()` implementieren (SD-Lese-Logik)
- `src/logic/runtime_config.h`: Ggf. Deklaration erweitern
- `src/main.cpp`: SD-Init vor `softConfigLoadOverrides()` sicherstellen
- Dokumentation: Dateiformat in Kommentar oder README

## Nicht-Scope (out)

- Kein WebUI für Credential-Eingabe (zukünftiges Feature)
- Kein Serial-Kommando zum Setzen von Credentials (zukünftiges Feature)
- Kein NVS-Speicher für Credentials (zukünftiges Feature)
- Kein Git-History-Cleanup (Mensch entscheidet separat über Credential-Rotation)

## Verifikation / Test

- `pio run -e T-ETH-Lite-ESP32S3` — muss kompilieren
- `pio run -e T-ETH-Lite-ESP32` — muss kompilieren
- `rg "oebhk\|0@AW\|euref-ip" include/` — keine Treffer in `soft_config.h`
- Ohne SD-Karte: Boot-Log zeigt Warnung, NTRIP startet nicht (erwartetes Verhalten)
- Mit SD-Karte + `/ntrip.cfg`: NTRIP-Config wird korrekt geladen

## Relevante ADRs

- **ADR-001** (Config-Schichtung): *"Beispiel-/Demo-Werte dürfen nicht stillschweigend produktive Defaults ersetzen."* → Wird erfüllt durch leere Defaults + dateibasiertes Laden.
- **ADR-001** (Config-Schichtung): *"Laufzeitänderungen dürfen fw_config nicht implizit überschreiben."* → Datei-basiertes Laden passiert in RuntimeConfig-Ebene, fw_config bleibt unverändert.

## Invarianten

- `RuntimeConfig` bleibt die einzige Quelle für NTRIP-Konfiguration zur Laufzeit
- `cfg::` Defaults in `soft_config.h` sind neutral (keine produktiven Daten)
- `softConfigLoadDefaults()` lädt weiterhin `cfg::` → `RuntimeConfig` ( leer)
- `softConfigLoadOverrides()` lädt Datei → `RuntimeConfig` (überschreibt leere Defaults)

## Known Traps

1. **SD-Karte muss in `setup()` gemountet werden:** Der maintTask mountet die SD-Karte erst bei Bedarf (Logging-Switch). Für Credentials-Lesen muss ein separater, früher SD-Zugriff in `setup()` passieren. Beachte SPI-Bus-Koordination: Auf ESP32-S3 teilt sich SD mit Sensor-SPI auf SPI2_HOST. Ein SD-Zugriff in `setup()` ist OK, da der maintTask noch nicht läuft.
2. **Dateiname:** Muss eindeutig und nicht kollidierend mit Log-Dateien sein (`/log_NNN.csv`). Vorschlag: `/ntrip.cfg` oder `/config/ntrip.ini`.
3. **ESP32 Classic vs. S3:** Der ESP32 Classic hat SD-Karte auf einem anderen SPI-Bus (HSPI). Der Entwickler muss sicherstellen, dass der SD-Zugriff für beide Targets funktioniert.
4. **Firmware ohne SD-Karte:** Wenn keine SD-Karte eingelegt ist, muss der Boot weiterhin erfolgreich abschließen. NTRIP wird einfach nicht konfiguriert (Host leer → NTRIP bleibt im IDLE-Zustand).
5. **MAX_PATH:** ESP32 FAT-FS hat `MAX_PATH = 256`. Dateiname sollte kurz gehalten werden.

## Merge Risk

- **Mittel:** Ändert den Boot-Pfad (SD-Zugriff in `setup()`). Auf Targets ohne SD-Karte kein Problem (Fallback). Auf Targets mit SD-Karte muss sichergestellt werden, dass der SPI-Bus-Zugriff korrekt koordiniert wird.

## Classification

- **category:** platform_reuse
- **priority:** high
- **delivery_mode:** firmware_only
- **exclusive_before:** Keine
- **parallelizable_after:** Parallel mit allen anderen TASK-03x

## Owner

- **Assigned:** KI-Entwickler
- **Status:** todo
