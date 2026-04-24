# TASK-001 Boot-Log validieren

- **ID**: TASK-001
- **Titel**: Boot-Log mit aktuellem Firmware-Stand validieren
- **Status**: open
- **Priorität**: high
- **Komponenten**: main.cpp, modules.cpp, control.cpp, Logging
- **Dependencies**: none
- **AC**:
  - Flash auf Zielhardware mit aktuellem Branch erfolgreich.
  - Boot-Log enthält `MODULES: === Hardware Detection ===`.
  - Boot-Log enthält `CTL: initialised (PID Kp=...)`.
  - Boot-Log enthält periodisches `STAT: hd=... st=...`.
  - Boot-Log enthält `tasks created, entering main loop`.
- **Owner**: firmware-team
- **Links**:
  - docs/Handover2.md#8-offene-aufgaben--todos
  - backlog/epics/EPIC-001-runtime-stability.md
- **delivery_mode**: hardware_required
