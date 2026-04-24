# TASK-019 Konsolidierung (Stand: 2026-04-17)

## Zuordnung gemergter Commits zu Backlog-Tasks

| Task | Merge-Commit | Kernergebnis |
|---|---|---|
| TASK-019A | `2da3450` | GNSS-UART-Pinbelegung zentralisiert (`hardware_pins.h`), HAL-Defaults + RX-Guard ergänzt |
| TASK-019B | `a8dff11` | Environment `gnss_buildup` in `platformio.ini` ergänzt |
| TASK-019C | `a66ca3e` | GNSS-Buildup-Initpfad (`hal_esp32_init_gnss_buildup`) + reduzierter Main-Flow |
| TASK-019D | `7191c17` | Nicht-blockierender UART1/2→Console-Mirror für Diagnose hinzugefügt |
| TASK-019E | `2334f48` | Smoke-Test-Reportstruktur standardisiert (`reports/TASK-019/...`) |

## Backlog-Bereinigung

- `TASK-019A..019E` wurden auf `done` gesetzt (Task-Dateien + `backlog/index.yaml` synchronisiert).
- `TASK-019` wurde auf `in_progress` gesetzt.
- Dependency-Referenz in `TASK-019` auf `TASK-014A` korrigiert (statt veralteter Zwischenbezeichnung `TASK-014`).

## Offene Restpunkte

Für die Schließung von `TASK-019` verbleiben Integrations-/Validierungsarbeiten:

1. produktive Dual-UM980-Failover-Logik,
2. Labor-/Feldvalidierung inkl. Umschalt- und Stabilitätsmetriken.

Dafür wurden zwei Folge-Tasks angelegt:
- `TASK-019F` – Dual-UM980 Failover-Logik,
- `TASK-019G` – Labor-/Feldvalidierung.

## Nächster Startpunkt für KI-Entwickler

1. `TASK-019F` übernehmen und Runtime-Umschaltlogik implementieren.
2. Danach `TASK-019G` mit Hardware-Setup, Smoke- und Feldtestreport durchführen.
3. Reports unter `reports/TASK-019/` ergänzen und Ergebnis gegen AC rückmelden.
