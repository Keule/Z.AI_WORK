# TASK-017 RTCM Validierung AgIO U-M980

- **ID**: TASK-017
- **Titel**: RTCM-Ende-zu-Ende Validierung mit AgIO und U-M980 durchführen
- **Status**: open
- **Priorität**: medium
- **Komponenten**: Integrationssetup AgIO, U-M980 Empfänger, Test-/Messprotokolle
- **Dependencies**: TASK-014, TASK-015, TASK-016
- **AC**:
  - Ende-zu-Ende Test zeigt stabile RTCM-Weiterleitung von UDP bis GNSS-Empfänger.
  - FixQuality/Age Werte in PGN-214 korrelieren mit beobachtetem Korrekturdatenzustand.
  - Testergebnisse und bekannte Grenzen sind nachvollziehbar dokumentiert.
- **Owner**: validation-team
- **Links**:
  - `backlog/tasks/TASK-014-hal-gnss-rtcm-uart-forwarding.md`
  - `backlog/tasks/TASK-015-udp-rtcm-receiver-und-buffering.md`
  - `backlog/tasks/TASK-016-pgn214-fixquality-age-integration.md`
  - `backlog/epics/EPIC-004-feature-expansion.md`
- **delivery_mode**: hardware_required
- **task_category**: feature_expansion
