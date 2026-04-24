# TASK-016 PGN214 FixQuality Age Integration

- **ID**: TASK-016
- **Titel**: PGN-214 um FixQuality/Age-Integration für RTCM-gestützte Korrekturdaten erweitern
- **Status**: open
- **Priorität**: medium
- **Komponenten**: PGN-214 Encoder/Decoder, GNSS-Statusmodell, Bridge-Integration
- **Dependencies**: TASK-013
- **AC**:
  - PGN-214 enthält die definierten Felder für FixQuality und Age gemäß Designvorgaben.
  - Der Mapping-Pfad von RTCM/GNSS-Status zu PGN-214 ist konsistent und testbar.
  - Rückwärtskompatibilität zu bestehenden Konsumenten ist dokumentiert oder abgesichert.
- **Owner**: firmware-team
- **Links**:
  - `backlog/tasks/TASK-013-rtcm-forwarder-design-und-schnittstellen.md`
  - `backlog/epics/EPIC-004-feature-expansion.md`
- **delivery_mode**: firmware_only
- **task_category**: feature_expansion
