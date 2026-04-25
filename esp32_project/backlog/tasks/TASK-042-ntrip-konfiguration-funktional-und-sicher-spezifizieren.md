# TASK-042 NTRIP-Konfiguration funktional und sicher spezifizieren

- **ID**: TASK-042
- **Titel**: NTRIP-Konfigurationspfad (Konsole/Runtime/Persistenz) funktional und sicher spezifizieren
- **Status**: open
- **Priorität**: high
- **Komponenten**: `src/logic/runtime_config.*`, `src/logic/ntrip.*`, `src/hal_esp32/esp32_web.*`, `src/hal_esp32/esp32_cli.*`, `src/main.cpp`, `include/soft_config.h`, `src/hal_esp32/sd_logger_esp32.cpp`
- **Dependencies**: TASK-037, TASK-038, TASK-025, TASK-033
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer
- **Epic**: EPIC-004

- **classification**: dependent
- **exclusive_before**: []
- **parallelizable_after**: []
- **Planungsnotiz (Dependencies/Parallelisierung)**:
  - `Dependencies` enthält nur harte Voraussetzungen (NTRIP-Basisintegration und dateibasierte Credential-Quelle).
  - `parallelizable_after` bleibt leer, da für diesen Spezifikations-Task kein separates Parallelisierungsfenster nach Teilvorgängern definiert ist.

- **Origin**:
  Nutzeranforderung aus Chat (2026-04-21): Ein neuer Task soll den NTRIP-Konfigurationspfad
  funktional und sicher spezifizieren, inkl. Menüführung, Validierung, Persistenz und Security-Regeln.

- **Diskussion**:
  - Direkt: https://chatgpt.com/codex/cloud/tasks/task_e_69e7aadc724c8325bde97fdc35e7544d

- **Kontext/Problem**:
  Der aktuelle NTRIP-Stand ist auf mehrere Tasks verteilt:
  - TASK-025: Grundintegration NTRIP-Client.
  - TASK-030: Modul-/maintTask-Integration und Statuskonsolidierung.
  - TASK-032: Status-Semantik (`CONNECTED != Fehler`) korrigiert.
  - TASK-033: Credentials aus Compile-Time-Defaults entfernt und dateibasiertes Laden eingeführt.

  Es fehlt weiterhin eine **vollständige, umsetzbare Spezifikation** für den operativen
  Konfigurationspfad (Konsole/Web/Runtime/Persistenz), insbesondere für sichere Eingabe,
  Validierung und klares Reboot-Verhalten.

- **Änderungsnotiz (2026-04-22)**: Zuordnung zu **EPIC-004 (Feature Expansion)**, weil Scope und Komponenten den operativen NTRIP-Konfigurationspfad (Konsole/Web/Runtime/Persistenz inkl. Security-Regeln) als auszubauende Produktfunktion definieren; die Wiederverwendung vorhandener Bausteine ist hier Mittel zum Zweck der Funktionsausweitung.

- **Pflicht-Hinweis vor Start**:
  Vor jeder Umsetzung dieses Tasks sind `README.md` und `agents.md` verpflichtend zu lesen
  und einzuhalten. Ohne diese Pflichtlektüre gilt die Umsetzung als Prozessabweichung.

- **Analyse bestehender NTRIP-Tasks (Konflikte/Lücken)**:
  1. **TASK-025 vs. TASK-033**:
     - TASK-025 beschreibt Konfiguration über Defaults in `soft_config.h`.
     - TASK-033 fordert leere Defaults + Datei-Override aus SD.
     - **Lücke**: Kein konsolidiertes Prioritätsmodell (z. B. Default < Datei < Konsole/Web < volatile Override).
  2. **TASK-030 vs. TASK-032**:
     - TASK-030 fordert NTRIP-Status in HW-Status.
     - TASK-032 behebt Semantikfehler im Success-Case.
     - **Lücke**: Keine ACs zur Fehlermeldungsqualität bei ungültiger Konfiguration (Input-Validation).
  3. **TASK-033 Security-Umfang**:
     - Credentials werden aus Quelltext entfernt.
     - **Lücke**: Keine explizite, testbare Regel zur Maskierung bei Konsole/Web und zu Log-Redaktion.
  4. **Persistenz/Boot-Verhalten**:
     - SD-basierte Overrides sind eingeführt, aber ein eindeutiges, dokumentiertes Verhalten für
       `save/reload/reboot` inkl. Fehlerfällen (SD fehlt, Datei defekt) ist nicht als vollständiger AC-Satz spezifiziert.

- **Scope (in)**:
  - Task-Spezifikation für vollständigen NTRIP-Konfigurationspfad erstellen und repo-normativ verankern.
  - Konsolidiertes Prioritäts-/Override-Modell für NTRIP-Parameter definieren.
  - ACs für UX (Menü), Security (Maskierung/Logs), Persistenz und Validierung festlegen.
  - Konkrete Dateifootprints (`files_read/files_write`) für Folge-Implementierung verbindlich benennen.

- **Nicht-Scope (out)**:
  - Keine direkte Implementierung von CLI/WebUI-Code in diesem Planungs-Task.
  - Keine neuen Netzwerkprotokolle oder TLS-Stack-Erweiterungen außerhalb der NTRIP-Konfiguration.
  - Kein Hardware-Redesign.

- **Pflichtlektüre vor Umsetzung**:
  1. `README.md`
  2. `agents.md`
  3. `docs/adr/ADR-001-code-as-truth.md`
  4. `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  5. `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md`
  6. `backlog/tasks/TASK-030-task025-backlog-status-und-ntrip-architektur-anpassung.md`
  7. `backlog/tasks/TASK-032-ntrip-hwstatus-bug-gnss-falscher-fehler.md`
  8. `backlog/tasks/TASK-033-ntrip-credentials-dateibasiertes-laden.md`

- **files_read**:
  - `README.md`
  - `agents.md`
  - `include/soft_config.h`
  - `src/logic/runtime_config.h`
  - `src/logic/runtime_config.cpp`
  - `src/logic/ntrip.h`
  - `src/logic/ntrip.cpp`
  - `src/main.cpp`
  - `src/hal_esp32/esp32_cli.cpp`
  - `src/hal_esp32/esp32_web.cpp`
  - `src/hal_esp32/sd_logger_esp32.cpp`
  - `docs/adr/ADR-001-code-as-truth.md`
  - `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  - `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md`
  - `backlog/tasks/TASK-030-task025-backlog-status-und-ntrip-architektur-anpassung.md`
  - `backlog/tasks/TASK-032-ntrip-hwstatus-bug-gnss-falscher-fehler.md`
  - `backlog/tasks/TASK-033-ntrip-credentials-dateibasiertes-laden.md`

- **files_write**:
  - `backlog/tasks/TASK-042-ntrip-konfiguration-funktional-und-sicher-spezifizieren.md`
  - `backlog/index.yaml`
  - (Folgetask-Implementierung, nicht Teil dieses Tasks):
    - `src/hal_esp32/esp32_cli.cpp`
    - `src/hal_esp32/esp32_web.cpp`
    - `src/logic/runtime_config.cpp`
    - `src/logic/runtime_config.h`
    - `src/main.cpp`

- **public_surface**:
  - `backlog/tasks/TASK-042-ntrip-konfiguration-funktional-und-sicher-spezifizieren.md`
  - `backlog/index.yaml`

- **merge_risk_files**:
  - `backlog/index.yaml`
  - `src/main.cpp`
  - `src/logic/runtime_config.cpp`
  - `src/hal_esp32/esp32_cli.cpp`
  - `src/hal_esp32/esp32_web.cpp`

- **risk_notes**:
  - Ohne klare Prioritätsreihenfolge können Konfigurationsquellen einander verdeckt überschreiben.
  - Unmaskierte Eingaben/Logs erzeugen direkte Credential-Leaks.
  - Unklare Persistenz-Semantik führt zu Fehlannahmen nach Reboot und erschwert Feldbetrieb.

- **Invarianten**:
  - NTRIP-Credentials werden nicht im normalen Log im Klartext ausgegeben.
  - `RuntimeConfig` bleibt die Laufzeitquelle; Persistenz ist explizit dokumentiert.
  - Validierungsfehler führen zu klaren Fehlermeldungen, ohne geheime Felder preiszugeben.

- **Known traps**:
  - Maskierung nur auf UI-Ebene reicht nicht; auch Debug/Status-Logs müssen redigiert werden.
  - „Optional TLS/CA“ darf nicht implizit „TLS erzwungen“ bedeuten; klare Fallback-/Fehlerregeln nötig.
  - Port-Validierung muss numerisch und im gültigen Bereich erfolgen (1..65535).

- **Rejected alternatives**:
  - „Security by convention“ ohne harte ACs:
    - verworfen, da nicht testbar.
  - Nur SD-Datei als Konfigurationsweg:
    - verworfen, da Konsole/Web als operative Eingabepfade explizit benötigt werden.

- **AC**:
  1. **Menüeinträge**:
     - Es sind Bedienpfade für NTRIP-Endpunkt (Host/Port/Mountpoint), Zugangsdaten (User/Pass)
       sowie optionale TLS-/CA-Parameter spezifiziert.
  2. **Maskierung sensibler Eingaben**:
     - Passwort und vergleichbar sensible Felder werden in Konsole/Web bei Eingabe und Anzeige maskiert.
  3. **Persistenz/Reload/Reboot**:
     - Das Verhalten bei Speichern, Neuladen und Reboot ist eindeutig dokumentiert
       (inkl. SD-fehlt/Datei-fehlerhaft-Fallback).
  4. **Validierung mit Fehlermeldungen**:
     - Pflichtfelder dürfen nicht leer sein (mind. Host, Mountpoint, User bei aktiviertem NTRIP),
       Port nur im Bereich `1..65535`, Fehlermeldungen sind klar und ohne Credential-Leak.
  5. **Security-Hinweise dokumentiert**:
     - Keine Klartext-Leaks im normalen Log; Umgang mit Credentials (Ablage, Anzeige, Änderung,
       Redaktionsregeln in Logs) ist als umsetzbare Vorgabe dokumentiert.
  6. **Task-Metadaten gepflegt**:
     - `classification`, `exclusive_before`, `parallelizable_after` sind gesetzt und konsistent.
  7. **Dateifootprint konkret**:
     - Relevante NTRIP-Konfigurationsdateien/-module sind unter `files_read/files_write` konkret benannt.

- **verification**:
  - Dokumenten-Review gegen TASK-025/030/032/033: Konflikte/Lücken explizit benannt.
  - Check, dass alle Pflicht-ACs als testbare Aussagen formuliert sind.
  - `backlog/index.yaml` enthält TASK-042 konsistent in `epics` und `tasks`.

- **Links**:
  - `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md`
  - `backlog/tasks/TASK-030-task025-backlog-status-und-ntrip-architektur-anpassung.md`
  - `backlog/tasks/TASK-032-ntrip-hwstatus-bug-gnss-falscher-fehler.md`
  - `backlog/tasks/TASK-033-ntrip-credentials-dateibasiertes-laden.md`
  - `docs/adr/ADR-001-code-as-truth.md`
  - `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
