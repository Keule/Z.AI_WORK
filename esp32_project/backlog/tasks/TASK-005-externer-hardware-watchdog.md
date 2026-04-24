# TASK-005 Externen Hardware-Watchdog ergänzen

- **ID**: TASK-005
- **Titel**: Externen Hardware-Watchdog spezifizieren und integrieren
- **Status**: open
- **Priorität**: medium
- **Komponenten**: hardware design, hal layer, main loop watchdog signaling
- **Dependencies**: TASK-001
- **AC**:
  - Watchdog-Hardwarebaustein und Ansteuerung definiert.
  - Firmware toggelt/bedient den externen Watchdog zyklisch.
  - Fehlerfall (ausbleibender Kick) ist reproduzierbar validiert.
  - Integration im Handover dokumentiert.
- **Owner**: firmware-team
- **Links**:
  - docs/Handover2.md#8-offene-aufgaben--todos
  - backlog/epics/EPIC-002-sensor-and-safety.md
- **delivery_mode**: mixed
