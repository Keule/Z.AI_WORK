# GNSS Buildup (normativ + operativ)

## Zweck des Environments `gnss_buildup`

`gnss_buildup` ist das dedizierte Diagnose-Buildprofil für den kontrollierten Start der GNSS/RTCM-Pfade (Dual-UM980-Basis) ohne vollständigen Steering-Vollausbau.

Ziel:
- reproduzierbarer Bringup von Ethernet + GNSS-UART,
- schnelle Sichtbarkeit von UART-Datenströmen im Console-Mirror,
- frühe Eingrenzung von Pin-/Wiring-/Portproblemen vor Feldtest.

## Aktivierte/Deaktivierte Feature-Sets

Aus `platformio.ini` (Environment `gnss_buildup`):

### Aktiviert
- `FEAT_PROFILE_COMM_ONLY`
- `FEAT_GNSS_BUILDUP`
- `FEAT_GNSS_UART_MIRROR`

### Parametrierung
- Keine Pin-/Baudwerte über `-D...` im Buildprofil.
- GNSS-Mirror-Pins und Baudrate sind zentral in `include/hardware_pins.h` definiert.

### Deaktiviert (implizit, da im Profil nicht gesetzt)
- Steering-Sensorik/Aktorik-Vollpfad
- IMU-Bringup (`FEAT_IMU_BRINGUP`)
- PID-Steuerpfad als Vollausbauprofil

## Finale UART-Pinbelegung (Board-seitig)

Verbindliche GNSS-UART-Zuordnung aus `include/hardware_pins.h`:

- UART1 (GNSS/RTCM primary): `TX=48`, `RX=45`
- UART2 (GNSS/diagnostic secondary): `TX=2`, `RX=1`

Board-Constraints:
- GPIO `26..37` reserviert (PSRAM)
- GPIO `38..42` output-only (nicht für UART RX)

## Start- und Smoke-Test-Reihenfolge

1. **Build**
   - `pio run -e gnss_buildup`
2. **Flash**
   - `pio run -e gnss_buildup -t upload`
3. **Monitor starten**
   - `pio device monitor -b 115200`
4. **Bootlog prüfen**
   - erwarteter Modus-Hinweis auf GNSS-Buildup
   - Mirror-Status (`enabled` + UART-Pin/baud Ausgabe)
5. **RTCM/UART-Funktion prüfen (Smoke)**
   - Daten auf UART1/2 in der Console sichtbar
   - keine dauerhaften Drop-/Overflow-Muster
6. **Timeout-/Fallback-Pfad prüfen**
   - bei ausbleibender Init muss degradierter Diagnosebetrieb geloggt werden (kein Hard-Abbruch)
7. **Ergebnis dokumentieren**
   - Report nach `TASK-019E`-Standard mit Verdict (`go` / `blocked` / `no-go`)

## Abgrenzung

Diese Seite ist die normative Betriebs- und Prüfbasis für `gnss_buildup`.
Handover-Dokumente dürfen ergänzen, aber nicht diese Vorgaben ersetzen.

## Grundsatz (verbindlich)

**Pinbelegung gehört in die zentrale Hardware-Konfiguration, nicht in Build-Flags.**

## Plattformregeln (ESP32 vs ESP32-S3, verbindlich)

### ESP32-WROVER-E (Environment `T-ETH-Lite-ESP32`)

Pflichtregeln:
- `board = esp-wrover-kit` (WROVER-E/PSRAM-Klasse; stabilere Basiskonfiguration als generisches `esp32dev`).
- Ethernet via Arduino-`ETH.h`/IDF-Driver mit W5500-Pfad, daher Build-Defines:
  - `ETH_PHY_TYPE=ETH_PHY_W5500`
  - `ETH_PHY_ADDR=1`
- Keine erzwungenen TFT_eSPI-Setup-Includes (`-include ...TFT_eSPI...`) im Ethernet/GNSS-Profil.

Warum:
- W5500 wird im Projekt explizit über `ETH.begin(...)` initialisiert; die PHY-Defines halten die Konfiguration im Buildprofil nachvollziehbar.
- TFT_eSPI ist für GNSS/ETH-Builds kein Pflichtbestandteil; erzwungene Includes führen unnötig zu Header-Abhängigkeiten.

### ESP32-S3 (Environment `T-ETH-Lite-ESP32S3`)

Pflichtregeln:
- S3-spezifische Build-Flags bleiben unverändert.
- Für GNSS-Buildup auf S3 wird ein eigener Pfad `gnss_buildup_s3` geführt.

Warum:
- So bleibt der bisherige S3-Workflow kompatibel, während `gnss_buildup` normativ den ESP32-WROVER-E-Pfad abdeckt.
