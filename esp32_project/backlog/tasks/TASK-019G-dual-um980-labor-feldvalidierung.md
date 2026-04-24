# TASK-019G Labor-/Feldvalidierung für Dual-UM980 durchführen

- **ID**: TASK-019G
- **Titel**: Labor- und Feldvalidierung für Dual-UM980 inklusive Smoke-/Langlauftests
- **Status**: open
- **Priorität**: high
- **Komponenten**: `reports/`, `docs/`, Testprotokolle, Feldtest-Checklisten
- **Dependencies**: TASK-019E, TASK-019F
- **AC**:
  - Labor-Smoke-Test deckt Boot, UART-Mirror, RTCM-Einspeisung und Failover-Fälle mit Evidenzlogs ab.
  - Feldtest enthält mindestens einen Ausfall-/Wiederkehrfall eines UM980 inklusive dokumentierter Reaktionszeit.
  - Messkriterien (Fix-Qualität, DiffAge, Umschaltlatenz, Datenlücken) sind für jede Testfahrt dokumentiert.
  - Abschlussreport enthält klares Verdict (`go`/`no-go`) und ggf. priorisierte Restpunkte.
- **Owner**: ki-planer
- **Links**:
  - `backlog/tasks/TASK-019-integrationsplanung-zwei-um980.md`
  - `backlog/tasks/TASK-019E-smoke-test-reportstandard.md`
  - `templates/dev-report.md`
- **delivery_mode**: hardware_required
- **task_category**: feature_expansion
