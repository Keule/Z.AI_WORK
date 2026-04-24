# TASK-015 UDP RTCM Receiver und Buffering

- **ID**: TASK-015
- **Titel**: UDP-basierten RTCM-Empfang mit robuster Pufferung implementieren
- **Status**: open
- **Priorität**: medium
- **Komponenten**: UDP Receiver, Netzwerk-Stack Integration, Ringbuffer/Queue
- **Dependencies**: TASK-013
- **AC**:
  - RTCM-Daten können per UDP empfangen und in den vorgesehenen Pufferpfad überführt werden.
  - Buffering-Strategie verhindert Datenverlust im Normalfall und behandelt Überlauf deterministisch.
  - Telemetrie/Status für Empfangsrate, Paketverlust und Bufferzustand ist verfügbar.
- **Owner**: firmware-team
- **Links**:
  - `backlog/tasks/TASK-013-rtcm-forwarder-design-und-schnittstellen.md`
  - `backlog/epics/EPIC-004-feature-expansion.md`
- **delivery_mode**: firmware_only
- **task_category**: feature_expansion
