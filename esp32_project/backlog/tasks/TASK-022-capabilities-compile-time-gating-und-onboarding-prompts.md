# TASK-022 Compile-Time-Capabilities nur bei Modulbedarf aktivieren + Pflicht-Onboarding in Codex-Task-Prompts

- **ID**: TASK-022
- **Titel**: Compile-Time-Gating für zusätzliche SPI/UART-Capabilities umsetzen und Pflicht-Onboarding Teil 1 in klickbare Codex-Task-Prompts integrieren
- **Status**: done
- **Priorität**: high
- **Komponenten**: Build-/Feature-Gating, Task-Übergabe-Prompts (klickbare Codex-Buttons), Capability-Header
- **Dependencies**: TASK-021
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer

- **classification**: dependent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Kontext/Problem**:
  - Zusätzliche SPI-Busse/UARTs sollen nicht mehr pauschal einkompiliert werden, sondern nur dann, wenn ein Modul diese Capability tatsächlich benötigt.
  - Die Prompts zur Codex-Aufgaberstellung (klickbare Buttons) müssen verpflichtend den Onboarding-Block Teil 1 erzwingen, damit KI-Entwickler vor Implementierung dieselben Prozessquellen lesen.

- **Scope (in)**:
  - Compile-Time-Feature-Gates für zusätzliche SPI/UART-Capabilities definieren bzw. vereinheitlichen.
  - Modulbedarf -> Capability-Makro-Mapping eindeutig machen (keine impliziten Aktivierungen).
  - Prompt-Template/-Generator für klickbare Codex-Task-Buttons erweitern: Pflicht-Onboarding Teil 1 muss im Prompttext enthalten sein (`README.md`, `docs/process/PLAN_AGENT.md`, `docs/process/QUICKSTART_WORKFLOW.md`).
  - Dokumentieren, welche Profile/Build-Flags welche Capabilities aktivieren.

- **Nicht-Scope (out)**:
  - Laufzeit-Initialisierung der Capabilities (Boot-Init-Logik).
  - Finale Pin-Claim-/Pin-Routing-Implementierung.

- **files_read**:
  - `platformio.ini`
  - `src/logic/features.h`
  - `src/main.cpp`
  - Stelle mit den klickbaren Codex-Task-Buttons bzw. Prompt-Templates (nach Strings `Codex`, `KI-Entwickler`, `TASK-` suchen)
  - `README.md`
  - `docs/process/PLAN_AGENT.md`
  - `docs/process/QUICKSTART_WORKFLOW.md`

- **files_write**:
  - `platformio.ini` (falls Build-Flags angepasst werden müssen)
  - `src/logic/features.h` (oder äquivalente Capability-Header)
  - Prompt-Datei(en) der klickbaren Codex-Task-Buttons / Prompt-Templates
  - ggf. begleitende Entwicklerdoku für die neue Compile-Time-Aktivierungslogik

- **risk_notes**:
  - Fehlendes Default-Mapping kann Features ungewollt deaktivieren.
  - Zu grobe `#ifdef`-Schnitte können bestehende Board-Profile brechen.
  - Onboarding-Text in Prompts darf nicht von UI-Logik abgeschnitten oder gekürzt werden.

- **AC**:
  - Für mindestens einen zusätzlichen SPI-Bus und einen zusätzlichen UART ist nachweisbar, dass sie nur bei explizitem Modulbedarf einkompiliert werden.
  - Build ohne diese Modul-Flags kompiliert weiterhin erfolgreich (keine Referenzen auf deaktivierte Capabilities).
  - Klickbare Codex-Task-Prompts enthalten verpflichtend den Onboarding-Hinweis auf:
    - `README.md`
    - `docs/process/PLAN_AGENT.md`
    - `docs/process/QUICKSTART_WORKFLOW.md`
  - Die Prompt-Anpassung ist in der UI/Promptquelle so umgesetzt, dass neue Entwickler-Aufgaben diesen Block standardmäßig mitgeben.

- **verification**:
  - `pio run` (mindestens Standardprofil + ein Profil mit aktivierter Zusatz-Capability)
  - Diff-/Text-Prüfung der Promptquelle auf Onboarding-Referenzen (`README.md`, `PLAN_AGENT.md`, `QUICKSTART_WORKFLOW.md`)
  - Kurzer Review-Nachweis im Entwickler-Report, welche Flags/Module welches Compile-Time-Gating auslösen

- **Links**:
  - `backlog/tasks/TASK-021-planungsauftrag-capability-init-und-pin-zuweisung.md`
  - `README.md`
  - `docs/process/PLAN_AGENT.md`
  - `docs/process/QUICKSTART_WORKFLOW.md`
