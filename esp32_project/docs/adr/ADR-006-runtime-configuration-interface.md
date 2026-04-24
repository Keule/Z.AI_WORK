# ADR-006: Laufzeit-Konfigurationsschnittstelle

- Status: accepted
- Datum: 2026-04-24
- Verwandte ADRs: ADR-001 (Config-Schichtung), ADR-005 (Betriebsmodus), ADR-003 (Feature-Module)

## Kontext

Die Firmware muss im Feld konfigurierbar sein, ohne dass ein Neukompilieren oder Laptop-Anschluss nötig ist. Aktuell existieren:
- Serial CLI mit Einzelkommandos (ntrip, net, pid, save, load)
- Setup-Wizard beim ersten Boot
- NVS-Persistenz für RuntimeConfig
- SD-Karte für NTRIP-Credentials (/ntrip.cfg)

Diese Ansätze sind fragmentiert, nicht einheitlich validiert und bieten keine konsistente Benutzererfahrung. Für den PAUSED Mode (ADR-005) wird ein einheitliches Konfigurations-Framework benötigt.

## Entscheidung

### Einheitliches Config-Framework

**Config-Kategorien** mit einheitlicher Schnittstelle:

| Kategorie | Felder | Validierung | Besonderheit |
|-----------|--------|-------------|--------------|
| Network | mode, IP, GW, Subnet, DNS | IP-Format, Subnetz-Maske | ETH-Reinit bei Änderung |
| NTRIP | host, port, mount, user, pass, reconnect | Port-Bereich, Host nicht leer | Passwort-Masking |
| GNSS | baud_a/b, role_a/b, swap_a/b | Baudraten-Validierung | UART-Reinit bei Änderung |
| PID | Kp, Ki, Kd | Wertebereich-Check | Sofort aktiv |
| Actuator | type, minPWM, maxPWM | Typ-Enum, PWM-Bereich | Typ-Wechsel erfordert Reinit |
| System | log_level, log_interval, actuator_type | Aufzählungs-Check | Sofort aktiv |

### Validierungs-Modell

Jede Kategorie hat:
1. `validate()` — Prüft alle Felder auf Gültigkeit, gibt Fehler zurück
2. `apply()` — Wendet Änderungen an (kann Hardware-Reinit triggern)
3. `save()` — Schreibt validierte Config atomar in NVS
4. `load()` — Lädt Config aus NVS
5. `show()` — Gibt aktuelle Werte aus (Passwörter maskiert)

### Atomic-Persist-Regel

Nur vollständige, validierte Konfigurationen werden persistiert:
1. Benutzer ändert Werte (nur im RAM)
2. `validate()` prüft alle Felder
3. Bei Erfolg: `save()` schreibt atomar in NVS
4. Bei Fehler: Änderungen verworfen, Fehlermeldung angezeigt

### Sicherheitsregeln

- NTRIP-Passwörter werden in logs und Serial-Ausgabe IMMER maskiert (`****`)
- Config-Änderungen sind NUR im PAUSED Mode möglich (ADR-005)
- Partielle/inkonsistente Configs werden abgelehnt
- SD-Karte /config.ini kann Defaults liefern (Überschreibt NVS beim Boot)

### Serial Menu Interface

Textbasiertes Menü im PAUSED Mode:
```
=== ZAI_GPS Konfiguration ===
1. Netzwerk
2. NTRIP
3. GNSS
4. PID Regler
5. Aktuator
6. System
S. Alle speichern
L. Von NVS laden
F. Werkseinstellungen
X. Beenden

Auswahl: _
```

### Web UI Interface (zukünftig)

RESTful API im WiFi AP Mode für dieselben Config-Kategorien. Gleiches Validierungs-Modell.

## Invarianten

- Keine Config-Änderung im ACTIVE Mode
- Partielle/ungültige Configs werden nie persistiert
- NTRIP-Passwörter erscheinen nie im Klartext in Logs oder Serial-Ausgabe
- Config-Änderungen mit Hardware-Auswirkung (Netzwerk, UART) triggern expliziten Reinit
- Die dreischichtige Config-Hierarchie (ADR-001) bleibt erhalten: fw_config → soft_config → RuntimeConfig

## Konsequenzen

### Positiv
- Einheitliche UX über Serial und Web UI
- Sichere Konfiguration durch Validierung und Atomic-Persist
- Feldkonfiguration ohne Laptop
- Erweiterbar für neue Kategorien

### Negativ
- Initialer Implementierungsaufwand
- Menu-System benötigt Serial-Buffer-Management
- Web UI erfordert zusätzlichen Speicher

### Alternativen

- Nur Serial CLI (aktueller Zustand)
  → Fragmentiert, keine Validierung, keine einheitliche UX
- JSON-Config auf SD-Karte
  → Erfordert SD-Karte, keine interaktive Konfiguration
- Nur Web UI
  → Erfordert WiFi-Infrastruktur, Serial-Fallback fehlt
