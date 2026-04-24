# TASK-002 DBG-Marker entfernen/abstufen

- **ID**: TASK-002
- **Titel**: Temporäre `[DBG-*]` Hz-Logs entfernen oder auf DEBUG-Level setzen
- **Status**: open
- **Priorität**: high
- **Komponenten**: main.cpp, control task, comm task, loop logging
- **Dependencies**: TASK-001
- **AC**:
  - `[DBG-CTRL]`, `[DBG-COMM]`, `[DBG-LOOP]` sind nicht mehr im normalen Laufzeitlog sichtbar.
  - Falls beibehalten, nur unter DEBUG/VERBOSE aktiv.
  - Keine Regression bei vorhandenen Diagnose-Logs.
- **Owner**: firmware-team
- **Links**:
  - docs/Handover2.md#8-offene-aufgaben--todos
  - backlog/epics/EPIC-001-runtime-stability.md
- **delivery_mode**: firmware_only
