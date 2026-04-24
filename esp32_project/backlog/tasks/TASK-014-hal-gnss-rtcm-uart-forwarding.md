# TASK-014 HAL GNSS RTCM UART Forwarding

- **ID**: TASK-014
- **Titel**: RTCM-Forwarding über GNSS-UART in der HAL implementieren
- **Status**: open
- **Priorität**: medium
- **Komponenten**: HAL, GNSS-UART Treiber, RTCM Forwarding-Pfad
- **Dependencies**: TASK-013
- **AC**:
  - RTCM-Daten können aus dem definierten Forwarder-Pfad über die GNSS-UART geschrieben werden.
  - UART-Fehlerfälle (Write-Fehler, Timeout, Backpressure) werden behandelt und als Status signalisiert.
  - Implementierung entspricht den in TASK-013 definierten Schnittstellen und Limits.
- **Owner**: firmware-team
- **Links**:
  - `backlog/tasks/TASK-013-rtcm-forwarder-design-und-schnittstellen.md`
  - `backlog/epics/EPIC-004-feature-expansion.md`
- **delivery_mode**: firmware_only
- **task_category**: feature_expansion
