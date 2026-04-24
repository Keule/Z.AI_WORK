# TASK-003 PGN 251 Konfigurationsbits anwenden

- **ID**: TASK-003
- **Titel**: Empfangene Steer-Config (PGN 251) funktional auf Hardware anwenden
- **Status**: open
- **Priorität**: high
- **Komponenten**: net.cpp, pgn_codec.cpp, actuator/steer configuration path
- **Dependencies**: none
- **AC**:
  - Relevante Bits (z. B. InvertWAS, RelayActiveHigh, MotorDriveDirection) werden nicht nur geloggt, sondern wirksam übernommen.
  - Änderung ist zur Laufzeit reproduzierbar testbar.
  - Dokumentation der angewendeten Bit-Mappings ergänzt.
- **Owner**: firmware-team
- **Links**:
  - docs/Handover2.md#8-offene-aufgaben--todos
  - backlog/epics/EPIC-001-runtime-stability.md
- **delivery_mode**: mixed
