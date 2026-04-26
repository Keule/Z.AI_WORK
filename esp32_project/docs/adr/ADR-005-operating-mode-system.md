# ADR-005: Betriebsmodus-System (Active / Paused)

> **⚠️ DEPRECATED — This ADR is superseded by ADR-007 (Two-Task Architecture).**
> The 3-state mode system (BOOTING → ACTIVE ↔ PAUSED) has been replaced by
> the 2-state model (CONFIG ↔ WORK) defined in `module_interface.h`.
> See ADR-007 and the Gesamtkonzept for the current design.
> The corresponding implementation files (`op_mode.h`, `op_mode.cpp`) have been deleted.
> This document is retained for historical reference only.

- Status: ~~superseded~~ **deprecated** — see ADR-007
- Datum: 2026-04-24
- Verwandte ADRs: ADR-002 (Taskmodell), ADR-003 (Feature-Module), ADR-STATE-001 (StateLock)

## Kontext

Die Firmware benötigt zwei Betriebsmodi für den Einsatz im Feld:
1. **ACTIVE** — Normale Autosteer-Operation mit Echtzeit-Regelung und Kommunikation
2. **PAUSED** — Konfiguration, Diagnose und Wartung ohne Echtzeitanforderungen

Aktuell existiert nur ein Boot-Maintenance-Mode (Safety Pin LOW → WiFi AP + BT SPP + Web OTA + CLI Session), der nur beim Boot erreicht wird. Eine laufzeitseitige Umschaltung ist nicht möglich. Im pausierten Betrieb soll zusätzlich Funktionalität zugänglich sein, die die Echtzeitschleifen beeinträchtigen würde (WiFi-Konfiguration, SD-Log-Export, umfassende Diagnose, Firmware-Update über WiFi).

## Entscheidung

### Zustandsmaschine

Drei Zustände: `BOOTING → ACTIVE ↔ PAUSED`

- **BOOTING**: Hardware-Initialisierung, Modul-Erkennung, Konfiguration laden
- **ACTIVE**: controlTask@200Hz + commTask@100Hz + maintTask(Hintergrund) laufen vollumfänglich
- **PAUSED**: controlTask und commTask angehalten (nur Watchdog-Feed), maintTask verstärkt (WiFi AP, BT SPP, SD Export, volle Diagnose)

### Übergangsbedingungen

| Übergang | Bedingung | Bemerkung |
|----------|-----------|-----------|
| BOOTING → ACTIVE | safety == HIGH nach Init | Normaler Start |
| BOOTING → PAUSED | safety == LOW nach Init | Boot-Maintenance |
| ACTIVE → PAUSED | safety == LOW UND speed < 0.1 km/h | Nur im Stillstand |
| PAUSED → ACTIVE | Manuelles Trigger + Sanity-Check | Module-Check |

### Trigger für Moduswechsel

- GPIO-Schalter (entprellt, 100ms)
- Serial CLI Kommando (`mode active` / `mode paused`)
- Web UI Button (nur in PAUSED erreichbar)
- AgIO-Kommando (zukünftig)

### Systemverhalten pro Modus

| Komponente | ACTIVE | PAUSED |
|------------|--------|--------|
| controlTask (200Hz) | Volle Pipeline | WDT-Feed nur |
| commTask (100Hz) | UDP/PGN/NTRIP | Angehalten |
| maintTask | SD Flush, ETH Monitor | WiFi AP, BT, SD Export, Diagnose |
| loop() | WDT, Telemetrie, PID Live | Volle CLI, Setup-Wizard, Selftest |
| WiFi/BT | AUS | AN (AP-Mode) |
| Serial CLI | Eingeschränkt | Vollzugriff + Config Menu |

### AgIO-Benachrichtigung

Bei Moduswechsel wird:
- Hello-PGN erneut gesendet mit aktualisiertem Status
- PGN 253 SwitchStatus-Bits aktualisiert
- Steuerungsdaten (PID-Output) auf 0 gesetzt im PAUSED

### Persistenz

Der letzte Modus wird in NVS gespeichert (Schlüssel: `op_mode_pref`). Beim Boot wird dieser Präferenzwert berücksichtigt, sofern der Safety-Pin den Übergang zulässt.

## Invarianten

- Watchdog ist in BEIDEN Modi aktiv
- Safety-Schaltung ist IMMER aktiv (kann nie deaktiviert werden)
- Ein Moduswechsel unterbricht niemals eine laufende SPI-Transaktion
- WiFi/BT sind nur im PAUSED Mode aktiv (kein RF-Interferenz mit Echtzeit-Pfaden)
- Im ACTIVE Mode kann keine Konfiguration geändert werden
- controlTask darf im PAUSED Mode keine SPI-Zugriffe durchführen

## Konsequenzen

### Positiv
- Feldkonfiguration ohne Laptop und USB-Kabel möglich
- Erweiterte Diagnose ohne Echtzeit-Einschränkungen
- Klare Trennung zwischen Betriebs- und Wartungsmodus
- Safety bleibt immer gewährleistet

### Negativ
- Zusätzliche Komplexität in der Task-Steuerung
- WiFi/BT-RF kann im PAUSED Mode Sensoren beeinflussen (akzeptabel da keine Regelung)
- Moduswechsel-Logik muss sorgfältig getestet werden

### Alternativen

- Nur Boot-Maintenance (aktueller Zustand)
  → Keine laufzeitseitige Konfiguration, Laptop immer nötig
- Einziger Modus mit Prioritätsystem
  → Zu komplex, Safety-Gefahr bei Konfiguration im Betrieb
- Web-UI dauerhaft aktiv
  → RF-Interferenz mit Echtzeit-Kommunikation, Speicher/Performance-Overhead
