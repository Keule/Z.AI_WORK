# ESP32_AGO_GNSS — Phase 0: Rapid Setup & Debug Infrastructure

> **Verwendung**: Dieser Plan beschreibt die Infrastruktur, die benötigt wird, um die grundlegenden Funktionalitäten schnell in Betrieb zu nehmen, zu konfigurieren und zu validieren — bevor Phase 1 (AgOpenGPS-Kompatibilität) umgesetzt wird.
>
> **Philosophie**: Alles soll über die Serial Console konfigurierbar und testbar sein. Kein Neuflashen für Konfigurationsänderungen. Kein AgOpenGPS nötig um das Board zu testen.

---

## 1. Ausgangslage

### 1.1 Was bereits existiert

| Feature | Status | Details |
|---------|--------|---------|
| Boot-Hardware-Detection | ✅ | Detaillierte Serial-Ausgabe aller Subsysteme |
| 5s-Telemetrie | ✅ | Heading, Winkel, Safety, Speed, PID, IMU, Netzwerk |
| Log-Level pro Modul (Compile-time) | ✅ | 12 Module mit einstellbaren Levels |
| Log-Level pro Modul (Runtime) | ✅ | `log <tag> <level>` Serial-Befehle |
| File:Line Filter | ✅ | `filter <file>[:line]` / `filter off` |
| WAS-Kalibrierung (interaktiv) | ✅ | 'c' bei Boot + Live-ADC-Anzeige |
| NTRIP-Config von SD-Card | ✅ | `/ntrip.cfg` INI-Format |
| HW-Fehlermonitoring | ✅ | 6 Subsysteme, PGN 0xDD + Serial |
| GNSS UART Mirror | ✅ | Compile-time aktivierbar |
| IMU Bringup-Modus | ⚠️ | Existiert, aber compile-time disabled |
| Modul-System (Runtime) | ✅ | 9 Module, Pin-Claims, Dependencies |

### 1.2 Was komplett fehlt

| Feature | Kritikalität |
|---------|-------------|
| Interaktives Setup-Menü / Command Shell | 🔴 Hoch |
| Runtime NTRIP-Config über Serial | 🔴 Hoch |
| Aktuator Test/Manual-Modus | 🔴 Hoch |
| PID-Tuning über Serial | 🟡 Mittel |
| Modul enable/disable über Serial | 🟡 Mittel |
| Runtime Netzwerk-Config | 🟡 Mittel |
| Sensor-Self-Test nach Boot | 🟡 Mittel |
| NVS Save/Load für Runtime-Config | 🟡 Mittel |
| FreRTOS Heap/Task-Diagnose | 🟢 Niedrig |
| Factory Reset | 🟡 Mittel |
| Help-Kommando | 🟡 Mittel |
| Ethernet/UDP Loopback-Test | 🟢 Niedrig |

### 1.3 Code-Referenzen

| Bereich | Datei | Ort |
|---------|-------|-----|
| Serial-Eingabe (Kalibrierung) | `src/main.cpp` | Zeilen 520-534 |
| Serial-Eingabe (Log-Befehle) | `src/main.cpp` | Zeilen 697-717 |
| Log-Befehl-Handler | `src/logic/log_ext.cpp` | Zeilen 69-160 |
| Log-Config (Levels) | `src/logic/log_config.h` | Komplett |
| 5s-Telemetrie | `src/main.cpp` | Zeilen 621-676 |
| RuntimeConfig Struct | `src/logic/runtime_config.h` | Zeilen 13-28 |
| RuntimeConfig Load | `src/logic/runtime_config.cpp` | Zeilen 37-171 |
| SD NTRIP Config | `src/logic/runtime_config.cpp` | Zeilen 93-153 |
| Soft Config Defaults | `include/soft_config.h` | Zeilen 15-49 |
| Modul-System | `src/logic/modules.h/cpp` | Komplett |
| NTRIP Config API | `src/logic/ntrip.h/cpp` | Zeilen 52-59, 274-277 |
| IMU Bringup-Modus | `src/logic/imu.cpp` | Zeile 17 (Gate), 108-276 (Logik) |
| ADR Bringup Modes | `docs/adr/subsystems/ADR-BUILD-001-bringup-and-diagnostic-modes.md` | Komplett |

---

## 2. Architektur-Entscheidungen

### 2.1 Command Shell Architektur

**Entscheidung**: Leichtgewichtige Command Shell im `loop()` integrieren. Kein separates RTOS-Task (vereinfacht, läuft auf Core 0, blockiert nicht die steuernden Tasks).

**Design**:
- Input-Buffer: 128 Byte Ringbuffer im `loop()` (nicht in commTask!)
- Parsing: Einfacher Tokenizer (Leerzeichen getrennt, max 8 Argumente)
- Dispatch: Tabelle von `{command, handler, help_text}` Einträgen
- Ausgabe: Direkt über `Serial.printf()` (kein HAL-Logging für Kommando-Ausgabe)

```
Eingabe: "ntrip set host caster.agopengps.de port 2101 mountpoint MYMOUNT"
         "actuator test pwm 50 dir 1"
         "pid show"
         "diag all"
         "save"
         "help"
```

**ADR**: Neuer ADR-005 "Serial Command Shell" empfohlen.

### 2.2 NVS Key-Value Store Erweiterung

**Entscheidung**: RuntimeConfig-Erweiterung um NVS-Persistenz. Bestehende NVS-Nutzung (nur WAS-Kalibrierung) auf alle relevanten Einstellungen erweitern.

**NVS-Keys**:
```
ntrip_host        : string (max 63)
ntrip_port        : uint16
ntrip_mountpoint  : string (max 47)
ntrip_user        : string (max 31)
ntrip_password    : string (max 31)
pid_kp            : float
pid_ki            : float
pid_kd            : float
net_ip            : uint32
net_gateway       : uint32
net_subnet        : uint32
actuator_type     : uint8
relay_polarity    : uint8
calib_was_min     : uint16
calib_was_max     : uint16
calib_was_center  : uint16
calib_counts_deg  : float
```

### 2.3 Menu-Eingabe

**Entscheidung**: Kein klassisches "Nummern-Menü" (1. ..., 2. ...). Stattdessen Command-basiert wie eine Shell. Das ist flexibler und erlaubt Skripting.

**Ausnahme**: Beim ersten Boot (keine NVS-Daten) wird ein Setup-Wizard gestartet, der die wichtigsten Parameter interaktiv abfragt. Dieser Wizard wird nur einmalig oder nach `factory`-Befehl ausgeführt.

---

## 3. Aufgaben

### 3.1 S0-01: Serial Command Shell Framework

**Aufwand**: M (2-3 Tage)

**Beschreibung**: Leichtgewichtiges Command-Processing-Framework implementieren, das in den bestehenden `loop()` integriert wird.

**Was**:
- Input-Buffer (128 Byte) im `loop()` mit `Serial.available()` / `Serial.read()`
- Zeilenbasierte Eingabe (CR/LF als Terminator)
- Tokenizer: Leerzeichen-getrennt, max 8 Tokens pro Zeile
- Command-Dispatch-Tabelle: `{const char* cmd, void (*handler)(int argc, char* argv[]), const char* help}`
- Built-in Kommandos: `help`, `version`, `uptime`, `free`, `tasks`, `restart`

**Dateien**:
- Neue: `src/logic/cli.h/cpp` — Command Line Interface
- Ändern: `src/main.cpp` — CLI-Integration in `loop()`, bestehende Log-Befehle migrieren

**Invarianten**:
- CLI darf NICHT auf I/O blockieren (kein TCP, kein SD, kein HAL-Blocking)
- CLI-Ausgabe erfolgt über `Serial.printf()` direkt (nicht über `hal_log()`)
- CLI läuft im Arduino `loop()` auf Core 0 (niedrigste Priorität)
- Bestehende Log-Befehle (`log`, `filter`) werden in das CLI-Framework migriert
- Kalibrierungs-Trigger ('c' bei Boot) bleibt bestehen (Boot-Phase vor CLI-Init)

**Known Traps**:
- `Serial.printf()` ist nicht thread-safe. commTask und loop() können gleichzeitig schreiben → Mutex für Serial-Output einführen oder buffer-basierte Ausgabe
- ESP32 USB CDC Serial hat Puffer-Limits. Bei großen Ausgaben (z.B. `diag all`) chunked output nutzen.

**Akzeptanzkriterien**:
- [ ] `help` zeigt alle verfügbaren Kommandos mit Kurzhilfe an
- [ ] `version` zeigt Firmware-Version, Build-Datum, Board-Profil
- [ ] `uptime` zeigt Laufzeit in hh:mm:ss
- [ ] `free` zeigt freien Heap, largest free block, PSRAM (falls vorhanden)
- [ ] `tasks` zeigt alle FreeRTOS-Tasks mit Stack-Highwater-Mark
- [ ] `restart` führt ESP32 Neustart durch
- [ ] Bestehende `log` und `filter` Befehle funktionieren weiterhin
- [ ] CLI blockiert weder controlTask noch commTask
- [ ] Command-Dispatch-Tabelle ist erweiterbar (einfach neue Einträge hinzufügen)

---

### 3.2 S0-02: NTRIP Runtime-Konfiguration über Serial

**Aufwand**: S (1-2 Tage)

**Beschreibung**: NTRIP-Verbindungseinstellungen über Serial ändern und testen, ohne SD-Card oder Neuflashen.

**Was**:
- `ntrip show` — Aktuelle Konfiguration anzeigen (Host, Port, Mountpoint, User, Password maskiert, Status)
- `ntrip set host <value>` — Hostname setzen
- `ntrip set port <value>` — Port setzen
- `ntrip set mountpoint <value>` — Mountpoint setzen
- `ntrip set user <value>` — Benutzername setzen
- `ntrip set password <value>` — Passwort setzen
- `ntrip connect` — NTRIP-Verbindung starten (überschreibt IDLE)
- `ntrip disconnect` — NTRIP-Verbindung trennen
- `ntrip status` — Zustand + Statistik anzeigen (Connected/Connecting/Error, Bytes empfangen, RTCM-Pakete, Uptime)
- `ntrip test` — Verbindungstest (connect, 5s warten, Status melden, disconnect)

**Integration**:
- `ntripSetConfig()` API in `ntrip.h/cpp` existiert bereits — nutzen!
- `ntripGetState()` API existiert bereits — nutzen!
- Config wird in RuntimeConfig gespeichert
- NTRIP State Machine in maintTask muss neuen Config-Wert erkennen

**Dateien**:
- `src/logic/ntrip.h/cpp` — State Machine: Reconnect bei Config-Änderung
- `src/logic/runtime_config.h` — Erweitern um Status-Tracking
- `src/logic/cli.h/cpp` — NTRIP-Handler registrieren

**Invarianten**:
- NTRIP connect/blocking darf NICHT in commTask oder controlTask passieren (maintTask!)
- Password wird bei `ntrip show` als `***` maskiert
- `ntrip set` während Connected → graceful disconnect, dann reconnect mit neuen Werten
- Bestehendes SD-Card `/ntrip.cfg` Loading bleibt bestehen (Serial-Werte überschreiben SD-Werte)

**Akzeptanzkriterien**:
- [ ] `ntrip show` zeigt alle Parameter und aktuellen Status
- [ ] `ntrip set host caster.example.com` ändert den Host
- [ ] `ntrip set password geheim` speichert das Passwort (nicht im Klartext loggen)
- [ ] `ntrip connect` startet die Verbindung über maintTask
- [ ] `ntrip disconnect` trennt die Verbindung
- [ ] `ntrip test` führt einen 5s-Verbindungstest durch
- [ ] Bestehende SD-Card Config wird weiterhin beim Boot geladen
- [ ] Änderungen werden bei `save` in NVS persistiert (S0-05)

---

### 3.3 S0-03: Aktuator Test-Modus

**Aufwand**: M (2-3 Tage)

**Beschreibung**: Aktuator über Serial manuell ansteuern zum Testen und Kalibrieren. Das ist essenziell um die Lenkung ohne AgOpenGPS zu validieren.

**Was**:
- `actuator type` — Aktuator-Typ anzeigen/setzen (`spi`, `cytron`, `ibt2`, `danfoss`)
- `actuator test pwm <0-65535>` — Direkten PWM-Wert senden (ohne PID)
- `actuator test dir <0|1>` — Richtung setzen (nur Cytron/IBT2)
- `actuator test center` — Aktuator auf Mittelstellung fahren (50% PWM)
- `actuator test stop` — Aktuator sofort stoppen (PWM = 0)
- `actuator sweep <min> <max> <delay_ms>` — Aktuator zwischen min/max PWM oszillieren lassen
- `actuator status` — Aktueller PWM-Wert, Richtung, Aktuator-Typ, Sicherheitsstatus
- `safety status` — Safety-Pin Status anzeigen
- `safety override <on|off>` — Safety temporär deaktivieren (NUR für Testzwecke, mit Warnung!)

**Sicherheitsaspekte**:
- `safety override on` erfordert Bestätigung durch nochmalige Eingabe
- Override wird automatisch nach 60 Sekunden deaktiviert (Timeout)
- Override wird bei jedem `actuator test` Befehl in der Console als WARNUNG angezeigt
- Override zählt als Diagnose-Modus — Steuerung über PID bleibt währenddessen deaktiviert

**Dateien**:
- `src/logic/actuator.h/cpp` — Erweiterung um manuelle Steuerung
- `src/hal/hal.h` — Neue HAL-Funktionen für manuelle Aktuator-Steuerung
- `src/hal_esp32/hal_impl.cpp` — ESP32 LEDC PWM Implementierung
- `src/logic/control.h/cpp` — PID-Loop pausieren wenn Test-Modus aktiv
- `src/logic/cli.h/cpp` — Aktuator-Handler registrieren

**Invarianten**:
- Aktuator-Test darf NUR im `loop()` (CLI-Handler) laufen, nie in controlTask
- PID-Controller muss pausiert werden wenn manueller Test aktiv (Flag in global_state)
- controlTask prüft Flag und überspringt Aktuator-Write wenn Test aktiv
- Safety-Override ist zeitlich begrenzt und mehrfach bestätigt

**Known Traps**:
- Aktuator ist aktuell nur ein SPI-Stub. Für echte Tests muss mindestens der SPI-Aktuator oder ein GPIO-PWM-Aktuator implementiert sein.
- ESP32 LEDC hat 16 Kanäle. Prüfen ob Kanäle frei sind.
- PWM-Frequenz muss zum Aktuator passen (meist 1-20 kHz).

**Akzeptanzkriterien**:
- [ ] `actuator test pwm 32000` sendet 50% PWM zum Aktuator
- [ ] `actuator test stop` setzt PWM auf 0
- [ ] `actuator sweep 10000 55000 500` oszilliert den Aktuator
- [ ] `actuator status` zeigt aktuellen Zustand
- [ ] `safety override on` erfordert Bestätigung und deaktiviert sich nach 60s
- [ ] PID-Loop ist während Test-Modus pausiert
- [ ] WARNUNG wird bei jedem Test-Befehl angezeigt

---

### 3.4 S0-04: PID-Tuning über Serial

**Aufwand**: S (1-2 Tage)

**Beschreibung**: PID-Parameter über Serial anzeigen und ändern. Ermöglicht schnelles Tuning ohne AgOpenGPS.

**Was**:
- `pid show` — Aktuelle PID-Parameter (Kp, Ki, Kd, MinPWM, HighPWM, CountsPerDegree, WASOffset, Ackerman, LowHighDegrees)
- `pid set kp <value>` — Proportional-Gain ändern
- `pid set ki <value>` — Integral-Gain ändern
- `pid set kd <value>` — Derivative-Gain ändern
- `pid set minpwm <value>` — Minimale PWM ändern
- `pid set highpwm <value>` — Maximale PWM ändern
- `pid live` — Schaltet erweiterte Live-Anzeige ein: Sollwinkel, Istwinkel, Fehler, PID-Output (10 Hz auf Serial)

**Integration**:
- `controlUpdateSettings()` in `control.h/cpp` existiert bereits — API nutzen
- PID-Parameter kommen aus `g_nav` (NavigationState)
- PGN 252 (SteerSettingsIn) überschreibt Serial-Werte wenn AgIO verbunden!

**Dateien**:
- `src/logic/control.h/cpp` — Neue `controlSetParams()` API
- `src/logic/global_state.h` — Parameter in NavigationState
- `src/logic/cli.h/cpp` — PID-Handler registrieren

**Invarianten**:
- Serial-Änderungen werden von PGN 252 überschrieben wenn AgIO verbunden (dokumentieren!)
- `pid live` erweiterte Ausgabe kann mit `pid live off` wieder deaktiviert werden
- Parameter-Änderungen werden bei `save` in NVS persistiert (S0-05)

**Akzeptanzkriterien**:
- [ ] `pid show` zeigt alle Parameter übersichtlich
- [ ] `pid set kp 2.5` ändert Kp sofort
- [ ] `pid live` zeigt 10 Hz Soll-Ist-Error-PWM Ausgabe
- [ ] PGN 252 überschreibt Serial-Werte (mit Log-Meldung)

---

### 3.5 S0-05: NVS-Persistenz für Runtime-Config

**Aufwand**: M (2-3 Tage)

**Beschreibung**: Runtime-Konfiguration in NVS (Non-Volatile Storage) speichern und beim Boot laden. Ermöglicht dauerhafte Konfigurationsänderungen über Serial ohne SD-Card.

**Was**:
- `save` — Aktuelle RuntimeConfig in NVS schreiben
- `load` — NVS-Config laden (überschreibt RAM-Werte, logged Änderungen)
- `factory` — Alle NVS-Werte löschen, Defaults laden. Erfordert Bestätigung (`factory confirm`)
- NVS-Ladereihenfolge beim Boot: NVS → soft_config Defaults → SD-Card Overrides → Serial-Werte

**NVS-Key-Schema**:
```
Namespace: "agsteer"
Keys:
  ntrip_host        : string [nvs_type=string]
  ntrip_port        : u16
  ntrip_mountpoint  : string
  ntrip_user        : string
  ntrip_password    : string (verschlüsselt? → TBD, für jetzt klartext mit Warnung)
  pid_kp            : u32 (float als uint32 bit-pattern)
  pid_ki            : u32
  pid_kd            : u32
  pid_minpwm        : u16
  pid_highpwm       : u16
  net_mode          : u8 (0=DHCP, 1=Static)
  net_ip            : u32
  net_gateway       : u32
  net_subnet        : u32
  actuator_type     : u8
```

**Dateien**:
- Neue: `src/logic/nvs_config.h/cpp` — NVS CRUD-Operationen
- Ändern: `src/main.cpp` — Boot-Sequence: NVS laden
- Ändern: `src/logic/cli.h/cpp` — `save`, `load`, `factory` Handler
- Ändern: `src/logic/runtime_config.cpp` — NVS-Integration

**Invarianten**:
- NVS-Laden passiert BEHALB der Modul-Aktivierung (Module brauchen Konfig)
- NVS-Corruption handling: Wenn NVS-Key nicht lesbar → Default aus soft_config.h
- WAS-Kalibrierungsdaten bleiben in eigenem NVS-Namespace (`was_calib`)
- `factory` löscht NUR den `agsteer` Namespace, NICHT die WAS-Kalibrierung

**Known Traps**:
- ESP32 NVS hat 6 KB maximale Key-Size pro Entry. Strings sind limitiert.
- NVS hat begrenzte Schreibzyklen (~100k). `save` sollte nur bei explizitem Befehl passieren, nicht automatisch.
- Passwort als Klartext in NVS ist ein Sicherheitsrisiko. Für Phase 0 akzeptabel, für Produktion muss verschlüsselt werden.

**Akzeptanzkriterien**:
- [ ] `save` speichert alle RuntimeConfig-Werte in NVS
- [ ] `load` lädt Werte aus NVS und überschreibt RAM
- [ ] `factory confirm` löscht NVS und lädt Defaults
- [ ] `factory` ohne `confirm` zeigt Warnung und verlangt Bestätigung
- [ ] Boot lädt NVS-Werte automatisch
- [ ] NVS-Corruption wird graceful gehandhabt (Fallback zu Defaults)
- [ ] WAS-Kalibrierung bleibt von `factory` unberührt

---

### 3.6 S0-06: Modul-Steuerung über Serial

**Aufwand**: S (1-2 Tage)

**Beschreibung**: Module über Serial aktivieren, deaktivieren und Status abfragen.

**Was**:
- `module list` — Alle Module mit Status (UNAVAILABLE/OFF/ON), Pin-Claims, Dependencies
- `module enable <name>` — Modul aktivieren (z.B. `module enable ntrip`)
- `module disable <name>` — Modul deaktivieren
- `module detect` — Hardware-Detection neu ausführen (nur für deaktivierte Module)
- `module pins <name>` — Zeigt Pin-Zuweisung des Moduls

**Modul-Namen** (CLI-spezifisch, lowercase):
`imu`, `ads`, `act`, `eth`, `gnss`, `ntrip`, `safety`, `logsw`, `sd`

**Integration**:
- `moduleActivate()` / `moduleDeactivate()` APIs in `modules.h/cpp` existieren bereits
- Pin-Claim-Logik ist bereits implementiert → einfach aufrufen

**Dateien**:
- `src/logic/modules.h/cpp` — `moduleDetectSingle()` API (falls nicht vorhanden)
- `src/logic/cli.h/cpp` — Module-Handler registrieren

**Invarianten**:
- ETH-Modul (Ethernet) kann NICHT deaktiviert werden (obligatorisch, ADR-003)
- `module enable act` ohne IMU+ADS → Fehlermeldung (Dependency nicht erfüllt)
- `module enable` mit Pin-Konflikt → Fehlermeldung mit Konflikt-Details
- Module-Änderungen wirken sich sofort aus (kein Restart nötig)

**Akzeptanzkriterien**:
- [ ] `module list` zeigt alle 9 Module übersichtlich
- [ ] `module enable ntrip` aktiviert NTRIP (wenn ETH aktiv)
- [ ] `module disable ntrip` deaktiviert NTRIP sauber (Disconnect, Pin-Release)
- [ ] `module enable act` ohne IMU zeigt Dependency-Fehler
- [ ] Pin-Konflikt wird sauber gemeldet

---

### 3.7 S0-07: Erweiterte Diagnose-Befehle

**Aufwand**: M (2-3 Tage)

**Beschreibung**: Diagnose-Befehle für Hardware-Tests, Netzwerk-Tests und System-Status.

**Was**:
- `diag hw` — Hardware-Detection neu ausführen (alle Module), Ergebnis anzeigen
- `diag imu` — IMU-Test: SPI-Kommunikation prüfen, Daten lesen (Heading, Roll, YawRate)
- `diag was` — WAS-Test: 20 ADC-Lesungen anzeigen, Rohwert + Grad
- `diag gnss` — GNSS-Test: 1s NMEA-Sätze von UART anzeigen, Fix-Quality, Satelliten
- `diag net` — Netzwerk-Test: IP, Subnet, Gateway, Link-Status, UDP-Socket-Status
- `diag ntrip` — NTRIP-Verbindungstest (wie `ntrip test`, aber ausführlicher)
- `diag sd` — SD-Card-Test: vorhanden, Größe, freier Platz, `/ntrip.cfg` Inhalt
- `diag mem` — Speicher-Übersicht: Heap, largest block, PSRAM, Stack-Watermarks aller Tasks
- `diag pgn` — Letzte 10 empfangene PGNs anzeigen (Typ, Src, Länge, Timestamp)

**Integration**:
- Viele HAL-Funktionen für Sensoren existieren bereits
- `hw_status` System existiert bereits → erweitern

**Dateien**:
- Neue: `src/logic/diag.h/cpp` — Diagnose-Handler
- `src/logic/cli.h/cpp` — diag-Handler registrieren
- `src/logic/net.h/cpp` — `netGetStatus()` API
- `src/logic/pgn_codec.cpp` — PGN-Receive-Logging (Ringbuffer, letzte 10)

**Invarianten**:
- `diag hw` darf NUR im `loop()` laufen (potentiell langsame Sensor-Operationen)
- `diag gnss` liest NMEA für 1s und gibt Buffer-Inhalt aus
- `diag pgn` braucht einen Ringbuffer für die letzten 10 PGNs (nur Metadata, kein Payload)

**Akzeptanzkriterien**:
- [ ] `diag hw` zeigt alle Subsysteme mit OK/FAIL
- [ ] `diag imu` zeigt Heading, Roll, YawRate (oder Fehler wenn nicht verbunden)
- [ ] `diag was` zeigt 20 ADC-Werte mit Umrechnung in Grad
- [ ] `diag gnss` zeigt letzte NMEA-Sätze
- [ ] `diag net` zeigt IP, Link-Status, Socket-Status
- [ ] `diag mem` zeigt Heap, PSRAM, Task-Stack-Watermarks

---

### 3.8 S0-08: Setup-Wizard für Erst-Inbetriebnahme

**Aufwand**: M (2-3 Tage)

**Beschreibung**: Automatischer Setup-Wizard beim ersten Boot (keine NVS-Daten vorhanden). Führt den Benutzer Schritt für Schritt durch die Grundkonfiguration.

**Was**:
- Wizard wird beim Boot gestartet wenn NVS-Namespace `agsteer` leer ist
- Wizard kann mit `setup` Befehl manuell gestartet werden (überschreibt NVS)
- Wizard fragt interaktiv ab:

```
=== AgSteer Setup-Wizard ===
Welcome! This wizard will guide you through the initial configuration.

Step 1/6: Network
  Current: DHCP (192.168.1.70)
  Use DHCP? [Y/n]: _
  > Static IP [192.168.5.70]: _
  > Subnet [255.255.255.0]: _
  > Gateway [192.168.5.1]: _

Step 2/6: NTRIP (RTCM Correction)
  Enable NTRIP? [y/N]: _
  > Caster host []: _
  > Port [2101]: _
  > Mountpoint []: _
  > Username []: _
  > Password []: _

Step 3/6: Actuator Type
  Available: spi, cytron, ibt2
  Select [spi]: _

Step 4/6: Steering Calibration
  Steer to LEFT mechanical stop and press ENTER...
  > ADC value: 1234 (reading...)
  Steer to RIGHT mechanical stop and press ENTER...
  > ADC value: 5678 (reading...)
  Steer to CENTER and press ENTER...
  > ADC value: 3456 (reading...)
  Calibration saved.

Step 5/6: IMU (optional)
  IMU detected: YES (BNO085)
  Use IMU? [Y/n]: _

Step 6/6: Summary
  Network:  192.168.5.70/24 GW=192.168.5.1
  NTRIP:    enabled (caster.example.com:2101/MYMOUNT)
  Actuator: cytron
  IMU:      enabled
  WAS:      calibrated (min=1234 max=5678 center=3456)

  Save configuration? [Y/n]: _
  Configuration saved to NVS.
  === Setup complete. Restarting... ===
```

**Dateien**:
- Neue: `src/logic/setup_wizard.h/cpp` — Wizard-Logik
- `src/logic/cli.h/cpp` — `setup` Befehl registrieren
- `src/main.cpp` — Boot-Check: NVS leer → Wizard starten

**Invarianten**:
- Wizard läuft im `loop()` — darf nicht in setup() blockieren (Hardware-Init muss vorher fertig sein)
- Wizard-Status-Flag: Wenn Wizard aktiv, controlTask und commTask starten NICHT (oder Paused)
- Abbruch: Ctrl+C oder `abort` beendet den Wizard ohne zu speichern
- Wizard speichert automatisch in NVS am Ende (kein separater `save` nötig)
- WAS-Kalibrierung im Wizard nutzt die bestehende Kalibrierungs-Logik

**Known Traps**:
- Kalibrierung erfordert Serial-Eingabe mit Timeout. Der Wizard muss graceful mit Timeout umgehen.
- Wizard-Status darf nicht verlorengehen wenn ESP32 resettet während Wizard läuft (Flag in NVS?)

**Akzeptanzkriterien**:
- [ ] Wizard startet automatisch bei leerem NVS
- [ ] `setup` startet Wizard manuell
- [ ] Wizard fragt Network, NTRIP, Actuator, WAS, IMU ab
- [ ] WAS-Kalibrierung im Wizard funktioniert
- [ ] Summary zeigt alle Einstellungen vor dem Speichern
- [ ] `save` speichert in NVS, `abort` bricht ohne Speichern ab
- [ ] Nach Wizard wird Neustart durchgeführt

---

### 3.9 S0-09: Erweiterte Boot- und Runtime-Debug-Ausgaben

**Aufwand**: S (1-2 Tage)

**Beschreibung**: Erweiterte Debug-Meldungen beim Boot und im Betrieb für schnellere Fehlersuche.

**Was**:

**Boot-Erweiterungen**:
- Boot-Zeit messen und anzeigen (Gesamt + pro Phase)
- NVS-Config beim Laden anzeigen: `NVS: loaded ntrip_host=caster.example.com`
- SD-Config beim Laden anzeigen: `SD-CFG: loaded /ntrip.cfg (5 keys)`
- Module-Aktivierung: Detaillierte Pin-Claim-Info pro Modul
- Task-Erstellung: Stack-Size und Priorität anzeigen

**Runtime-Erweiterungen**:
- PGN-RX-Logging: Jede empfangene PGN in Kurzform loggen (wenn `log pgn debug` aktiv)
  - Format: `PGN-RX: [254] SteerDataIn from 0x7F len=8`
- PGN-TX-Logging: Jede gesendete PGN in Kurzform loggen (wenn `log pgn debug` aktiv)
  - Format: `PGN-TX: [253] SteerStatusOut to 0x7F len=15`
- NTRIP State-Change: `NTRIP: IDLE → CONNECTING` bei jedem Zustandswechsel
- Safety-Change: `SAFETY: OK → KICK` bei jedem Wechsel
- Watchdog-Events: `WDOG: reset (last PGN 254: 3200ms ago)`
- Control-Loop-Statistik: alle 30s → `CTL: avg_cycle=4.98ms max_cycle=6.21ms`

**Dateien**:
- `src/main.cpp` — Boot-Erweiterungen
- `src/logic/net.h/cpp` — PGN TX/RX Logging
- `src/logic/ntrip.cpp` — State-Change Logging
- `src/logic/control.h/cpp` — Control-Loop-Statistik
- `src/logic/hw_status.cpp` — Safety-Change Logging

**Invarianten**:
- Erweiterte Boot-Ausgabe ist immer aktiv (kein Gate)
- PGN-Logging ist an `log pgn debug` gebunden (default: INFO → kein PGN-RX/TX Logging)
- NTRIP/Safety/WDOG Logging ist immer auf WARN-Level (wichtige Events immer sichtbar)
- Control-Loop-Statistik zählt nur wenn `log ctl debug` aktiv (Performance-Impact minimal)

**Akzeptanzkriterien**:
- [ ] Boot zeigt Gesamtdauer und pro-Phase Zeiten
- [ ] NVS/SD Config-Laden wird geloggt
- [ ] `log pgn debug` aktiviert PGN-RX/TX Logging
- [ ] NTRIP State-Changes werden geloggt
- [ ] Safety-Changes werden geloggt
- [ ] Control-Loop-Statistik verfügbar bei debug-Level

---

### 3.10 S0-10: Netzwerk-Konfiguration über Serial

**Aufwand**: S (1-2 Tage)

**Beschreibung**: Netzwerkeinstellungen (IP, Subnet, Gateway) über Serial ändern. Erforderlich um das Board an verschiedene Netzwerke anzupassen.

**Was**:
- `net show` — Aktuelle IP, Subnet, Gateway, DNS, MAC, Link-Status
- `net mode <dhcp|static>` — DHCP oder Static umschalten
- `net ip <address>` — Statische IP setzen (z.B. `net ip 192.168.5.70`)
- `net gateway <address>` — Gateway setzen
- `net subnet <mask>` — Subnet-Maske setzen
- `net restart` — Netzwerk neu initialisieren (mit neuen Einstellungen)
- `net ping <host>` — Einfacher ICMP-Ping (oder UDP-Ping zum AgIO-Port)

**Dateien**:
- `src/hal/hal.h` — `hal_net_set_config()`, `hal_net_restart()` APIs
- `src/hal_esp32/hal_impl.cpp` — ESP32 Netzwerk-Rekonfiguration
- `src/logic/runtime_config.h` — Netzwerk-Config Felder
- `src/logic/cli.h/cpp` — net-Handler registrieren

**Invarianten**:
- `net restart` darf NUR im `loop()` aufgerufen werden (blocking!)
- Netzwerk-Änderung während AgIO-Verbunden → Verbindungsabbruch, automatischer Reconnect nach Restart
- DHCP-Modus ignoriert statische IP-Einstellungen
- Änderungen werden bei `save` in NVS persistiert

**Known Traps**:
- ESP32 Ethernet (W5500/RMII) muss komplett reinitialisiert werden bei IP-Wechsel
- DHCP-Timeout kann bis zu 30 Sekunden dauern → `net restart` blockiert den loop() entsprechend
- MAC-Adresse ist fest (im ESP32/Board einprogrammiert), nicht änderbar

**Akzeptanzkriterien**:
- [ ] `net show` zeigt alle Netzwerk-Parameter
- [ ] `net mode static` + `net ip 192.168.5.70` + `net restart` konfiguriert statische IP
- [ ] `net mode dhcp` + `net restart` fordert DHCP an
- [ ] Netzwerk-Config wird bei `save` persistiert

---

## 4. Abhängigkeiten und Reihenfolge

```
S0-01 (CLI Framework)
  ├── S0-02 (NTRIP Config)
  ├── S0-03 (Aktuator Test)
  ├── S0-04 (PID Tuning)
  ├── S0-05 (NVS Persistenz)
  ├── S0-06 (Modul-Steuerung)
  ├── S0-07 (Diagnose)
  ├── S0-08 (Setup-Wizard) ── abhängig von S0-02, S0-05, S0-10
  ├── S0-09 (Debug-Ausgaben)
  └── S0-10 (Netzwerk-Config)
```

**Parallelisierbar** (nach S0-01):
- S0-02, S0-03, S0-04, S0-06, S0-07, S0-09, S0-10 können parallel entwickelt werden
- S0-05 sollte vor S0-08 fertig sein
- S0-08 ist der letzte Task (integrative, nutzt alle anderen)

**Kritischer Pfad**: S0-01 → S0-05 → S0-08

---

## 5. Aufwandsschätzung

| ID | Task | Aufwand | Abhängig von |
|----|------|---------|-------------|
| S0-01 | CLI Framework | 2-3 Tage | — |
| S0-02 | NTRIP Config | 1-2 Tage | S0-01 |
| S0-03 | Aktuator Test | 2-3 Tage | S0-01 |
| S0-04 | PID Tuning | 1-2 Tage | S0-01 |
| S0-05 | NVS Persistenz | 2-3 Tage | S0-01 |
| S0-06 | Modul-Steuerung | 1-2 Tage | S0-01 |
| S0-07 | Diagnose | 2-3 Tage | S0-01 |
| S0-08 | Setup-Wizard | 2-3 Tage | S0-02, S0-05, S0-10 |
| S0-09 | Debug-Ausgaben | 1-2 Tage | S0-01 |
| S0-10 | Netzwerk-Config | 1-2 Tage | S0-01 |

| Phase | Gesamt |
|-------|--------|
| Sequential (kritisch Pfad) | 7-11 Tage |
| Parallel (alle Tasks) | 3-4 Wochen |
| **Empfehlung** | **2-3 Wochen** |

---

## 6. Quick Wins (innerhalb von 1-2 Tagen umsetzbar)

Folgende Tasks können sofort umgesetzt werden, ohne den großen CLI-Framework zu bauen:

### QW-01: Boot-Zeit-Messung (1 Stunde)
- `uint32_t t0 = millis();` am Anfang von `setup()`
- Nach jeder Phase: `hal_log(INFO, "BOOT", "phase X: %lu ms", millis() - t_phase);`
- Am Ende: `hal_log(INFO, "BOOT", "total: %lu ms", millis() - t0);`

### QW-02: Help-Kommando (1 Stunde)
- `if (strcmp(cmd, "help") == 0)` in bestehende Serial-Verarbeitung einfügen
- Liste: `log <tag> <level>`, `log status`, `filter <file[:line]>`, `filter off`
- Datei: `src/main.cpp:697-717`

### QW-03: NTRIP Status-Kommando (2 Stunden)
- `ntrip status` Befehl: `ntripGetState()` aufrufen und formatiert ausgeben
- Zeigt: State, Host, Port, Mountpoint, Bytes RX, RTCM Pakete, Uptime
- Datei: `src/main.cpp` (Serial-Handler), `src/logic/ntrip.h` (API)

### QW-04: `version` und `free` Kommandos (1 Stunde)
- `version`: Build-Info ausgeben (existiert als Makros im Code)
- `free`: `ESP.getFreeHeap()`, `ESP.getHeapSize()`, `ESP.getFreePsram()`
- Datei: `src/main.cpp`

### QW-05: PGN-RX-Debug-Logging (2 Stunden)
- In `netProcessFrame()` nach Decode: `hal_log(DEBUG, "PGN-RX", "[%d] from 0x%02X len=%d", pgn, src, len);`
- Gate: Nur wenn `log pgn debug` aktiv
- Datei: `src/logic/net.cpp`

### QW-06: Safety-State-Change-Log (30 Minuten)
- In controlStep() bei Safety-Wechsel: `hal_log(WARN, "SAFETY", "OK -> KICK" oder "KICK -> OK");`
- Datei: `src/logic/control.cpp`

---

## 7. Migration: Bestehende Log-Befehle

Die bestehenden Log-Befehle in `src/logic/log_ext.cpp` und `src/main.cpp:697-717` werden in das CLI-Framework (S0-01) migriert. Die Funktionalität bleibt identisch, nur der Dispatch-Mechanismus ändert sich.

**Bestehend**:
```
log <tag> <level>       → cli_handler_log(argc, argv)
log all <level>         → cli_handler_log_all(argc, argv)
log status              → cli_handler_log_status(argc, argv)
filter <file[:line]>    → cli_handler_filter(argc, argv)
filter off              → cli_handler_filter_off(argc, argv)
```

**Neu (nach Migration)**:
```
help                    → cli_handler_help(argc, argv)
version                 → cli_handler_version(argc, argv)
uptime                  → cli_handler_uptime(argc, argv)
free                    → cli_handler_free(argc, argv)
tasks                   → cli_handler_tasks(argc, argv)
restart                 → cli_handler_restart(argc, argv)
log <tag> <level>       → (migriert)
filter <file[:line]>    → (migriert)
... (alle neuen Befehle)
```

---

## 8. Sichere Default-Konfiguration

Wenn der Setup-Wizard übersprungen wird (z.B. NVS-Daten vorhanden) oder keine Konfiguration gemacht wurde, gelten folgende sichere Defaults:

| Parameter | Default | Begründung |
|-----------|---------|------------|
| Netzwerk | DHCP | Funktioniert in den meisten Netzwerken |
| NTRIP | Deaktiviert | Ohne Config nicht verbinden (leerer Host → IDLE) |
| Aktuator-Typ | SPI | Standard-Aktuator |
| PID Kp | 1.0 | Konservativer Default |
| PID Ki | 0.0 | Kein Integral-Anteil (verhindert Windup) |
| PID Kd | 0.01 | Minimaler Derivative-Anteil |
| Safety | Pflicht (nicht deaktivierbar ohne expliziten Befehl) | Sicherheit geht vor |
| Log-Level | INFO | Ausreichend Info, kein Debug-Flut |

---

## 9. Risiken

| Risiko | Auswirkung | Mitigation |
|--------|-----------|------------|
| Serial-Output Race Condition | Korrupte Ausgabe wenn loop() und commTask gleichzeitig schreiben | Serial-Mutex oder buffer-basierte Ausgabe |
| NVS Corruption bei Reset während `save` | Konfiguration verloren | NVS commit atomar, Flag-Vorher/Nachher |
| Aktuator Test ohne Safety | Mechanische Beschädigung | Safety-Override erfordert Bestätigung + Timeout |
| Setup-Wizard bei fehlendem Serial-Monitor | Boot hängt | Wizard-Timeout (30s pro Schritt), dann Defaults |
| CLI-Befehle während controlTask läuft | Inkonistente Zustände | Globaler Test-Mode-Flag, controlTask prüft Flag |

---

## 10. Empfohlene Arbeitsreihenfolge

### Woche 1: Foundation
1. **S0-01**: CLI Framework (2-3 Tage)
2. **QW-01 bis QW-06**: Quick Wins parallel (1-2 Tage)
3. **S0-09**: Erweiterte Debug-Ausgaben (1 Tag, kann parallel zu QWs)

### Woche 2: Core Features
4. **S0-05**: NVS Persistenz (2-3 Tage)
5. **S0-02**: NTRIP Config (1-2 Tage, parallel zu S0-05 möglich)
6. **S0-10**: Netzwerk-Config (1-2 Tage, parallel)
7. **S0-04**: PID Tuning (1-2 Tage, parallel)

### Woche 3: Advanced
8. **S0-03**: Aktuator Test (2-3 Tage)
9. **S0-07**: Diagnose-Befehle (2-3 Tage, parallel)
10. **S0-06**: Modul-Steuerung (1-2 Tage, parallel)
11. **S0-08**: Setup-Wizard (2-3 Tage, nach S0-02/S0-05/S0-10)
