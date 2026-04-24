# TASK-039 Ethernet-Konfiguration im seriellen Konfigmodus

- **ID**: TASK-039
- **Titel**: Ethernet-Netzwerkmodus und IP-Parameter im seriellen Konfigmodus konfigurierbar machen
- **Status**: open
- **Priorität**: high
- **Komponenten**: `src/main.cpp`, `src/logic/runtime_config.*`, `include/soft_config.h`, `src/logic/modules.*`, `src/logic/hw_status.cpp`, Serial-Konfigpfad
- **Dependencies**: TASK-037, TASK-038, TASK-028, TASK-030
- **delivery_mode**: firmware_only
- **task_category**: feature_expansion
- **Owner**: ki-planer
- **Epic**: EPIC-004

- **classification**: dependent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Origin**:
  Nutzeranforderung aus Chat (2026-04-21): Task fuer Ethernet-Konfiguration im seriellen Konfigmodus erstellen, inkl. DHCP/statisch, IP-Validierung, Diagnoseanzeige und Recovery-Pfad.

- **Diskussion**:
  - Direkt: CLI-Chat-Sitzung vom 2026-04-21 (keine persistente URL im Repository-Kontext verfuegbar)

- **Kontext/Problem**:
  Aktuell sind Netzparameter in Defaults/Code verteilt und der serielle Konfigmodus bietet keine konsistente, abgesicherte Ethernet-Konfiguration fuer DHCP-vs.-statisch inklusive Diagnostik. Fuer NTRIP und weitere netzabhaengige Funktionen muss ein reproduzierbarer, robust validierter Netzwerkpfad vorhanden sein, der auch bei Fehlkonfiguration recoverbar bleibt.

- **Änderungsnotiz (2026-04-22)**: Zuordnung zu **EPIC-004 (Feature Expansion)**, weil der Scope neue nutzerseitige Ethernet-Konfigurationsfunktionen (DHCP/statisch, Validierung, Diagnose, Recovery) für den Serial-Konfigmodus ergänzt und damit primär eine Funktionsausweitung statt reine Plattform-Wiederverwendung adressiert.

- **Scope (in)**:
  - Serielle Konfigoption fuer Netzwerkmodus (`DHCP` vs. `statisch`) einfuehren oder konsolidieren.
  - Bei statischer Konfiguration die Felder `IP`, `Subnetz`, `Gateway`, `DNS` im Serial-Menue fuehren (soweit im Projektpfad unterstuetzt); falls einzelne Felder technisch nicht unterstuetzt sind, dies explizit ausweisen.
  - Eingabevalidierung fuer IPv4-Parameter im Serial-Pfad (Format + semantische Plausibilitaet) umsetzen.
  - Link-Status/Diagnoseanzeige im Serial-Menue ergaenzen (mindestens Link up/down, IP-Status, letzter Fehlergrund falls vorhanden).
  - Recovery-Pfad fuer Fehlkonfiguration dokumentieren und implementieren (Reset auf DHCP/Defaults per Serial-Konsole ohne Reflash).
  - Abgleich mit bestehenden Netzwerk-/Ethernet-Aufgaben herstellen, insbesondere:
    - `TASK-020` (ETH/Treiber-/Integrationskontext),
    - `TASK-025`/`TASK-030` (NTRIP-Netzabhaengigkeit und Laufzeitintegration),
    - `TASK-032` (Status-/Diagnosekonsistenz im hwStatus-Pfad).

- **Nicht-Scope (out)**:
  - Keine WebUI fuer Ethernet-Konfiguration.
  - Keine NVS-/Dateipersistenz ausserhalb bereits vorhandener RuntimeConfig-Pfade.
  - Kein Umbau des physischen ETH-Treiberstacks oder Board-Pinout-Designs.

- **Pflichtlektuere vor Umsetzung**:
  - `README.md` und `agents.md` lesen und strikt einhalten.
  - `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  - `backlog/tasks/TASK-020-tft-espi-und-lilygo-eth-lite-treiber-integration-planen.md`
  - `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md`
  - `backlog/tasks/TASK-030-task025-backlog-status-und-ntrip-architektur-anpassung.md`

- **files_read**:
  - `include/soft_config.h`
  - `src/logic/runtime_config.h`
  - `src/logic/runtime_config.cpp`
  - `src/main.cpp`
  - `src/logic/modules.h`
  - `src/logic/modules.cpp`
  - `src/logic/hw_status.cpp`
  - `src/hal/hal.h`
  - `src/hal_esp32/hal_impl.cpp`
  - `backlog/tasks/TASK-020-tft-espi-und-lilygo-eth-lite-treiber-integration-planen.md`
  - `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md`
  - `backlog/tasks/TASK-030-task025-backlog-status-und-ntrip-architektur-anpassung.md`
  - `backlog/tasks/TASK-032-ntrip-hwstatus-bug-gnss-falscher-fehler.md`

- **files_write**:
  - `src/main.cpp` (Serial-Menue-Flows fuer ETH-Konfiguration + Recovery-Command)
  - `src/logic/runtime_config.h` (Ethernet-Runtime-Felder fuer DHCP/statisch, IP/Subnetz/Gateway/DNS)
  - `src/logic/runtime_config.cpp` (Defaults/Load- und Validierungsfluss)
  - `include/soft_config.h` (projektspezifische Defaultwerte fuer ETH-Konfiguration)
  - `src/logic/hw_status.cpp` (Link-/Diagnoseanzeige fuer Serial-Menue und Statuspfad)
  - `reports/TASK-039/<dev-name>.md` (Umsetzungs-/Validierungsreport)

- **public_surface**:
  - `src/logic/runtime_config.h` (oeffentliche Runtime-Konfigstruktur fuer Ethernet-Parameter)
  - `include/soft_config.h` (oeffentliche Nutzer-Defaults fuer ETH-Modus/Netzparameter)
  - `src/main.cpp` (oeffentliche Serial-Konfigkommandos/Prompt-Texte fuer Ethernet)

- **merge_risk_files**:
  - `src/main.cpp` (hohes Konfliktrisiko durch parallele Task-Aenderungen im Boot-/Serial-/Task-Orchestrierungspfad)
  - `src/logic/runtime_config.cpp` (Konfig-Ladepfad, oft parallel betroffen)
  - `src/logic/hw_status.cpp` (Diagnose-/Statuspfad, Risiko auf inkonsistente Fehlerflags)

- **risk_notes**:
  - Boot-Blockade-Risiko durch Netzwerk-Waits beim Wechsel DHCP↔statisch; darf setup()/maintTask nicht unzulaessig blockieren.
  - Timing-Risiko bei ETH-Link-Flaps waehrend Re-Konfiguration; Reconnect-Strategie muss Backoff/Retry klar begrenzen.
  - Falsch-positive „connected“-Zustaende koennen NTRIP-Folgelogik triggern, obwohl IP-Konfiguration ungültig ist.
  - Recovery-Pfad muss auch bei komplett ungueltiger statischer Konfiguration erreichbar bleiben (kein Lockout).

- **Invarianten**:
  - Ohne valide Netzkonfiguration darf kein Zustand als „ETH ready“ publiziert werden.
  - Serial-Konfiguration darf bestehende Modul- und Dependency-Gates (`ETH`, `NTRIP`) nicht umgehen.
  - Der Recovery-Pfad ueber die Serial-Konsole bleibt immer verfuegbar, auch nach Fehlkonfiguration.

- **Known traps**:
  - DHCP↔statisch-Umschaltung kann stale Socket-/Interface-Zustaende hinterlassen, wenn NTRIP/ETH nicht sauber neu initialisiert werden.
  - DNS-Feld wird oft vergessen zu validieren oder zu uebernehmen, obwohl Namensaufloesung fuer NTRIP benoetigt wird.
  - Statusanzeige muss zwischen Link-Layer-Status und IP-Layer-Status unterscheiden.

- **Rejected alternatives**:
  - Nur Compile-Time-Netzwerkmodus in `soft_config.h` ohne Serial-Umschaltung:
    - verworfen, da Anforderung explizit Serial-Konfigmodus fordert.
  - Silent fallback von statisch auf DHCP bei Validierungsfehlern:
    - verworfen, da reproduzierbares Fehlverhalten und Benutzerfeedback (Abweisung) gefordert sind.

- **AC**:
  - Umschalten zwischen DHCP und statischer Konfiguration funktioniert reproduzierbar ueber den seriellen Konfigmodus.
  - Ungueltige IP-/Subnetz-/Gateway-/DNS-Parameter werden vom Serial-Konfigpfad abgewiesen und als Fehler rueckgemeldet.
  - Link-Status und Netzdiagnose sind im Serial-Menue sichtbar (inkl. klarer Trennung Link vs. IP-Status).
  - Bei Fehlkonfiguration existiert ein dokumentierter und funktionierender Recovery-Pfad ueber die Serial-Konsole.
  - Abgleich mit `TASK-020` sowie NTRIP-bezogenen Netzabhaengigkeiten (`TASK-025`/`TASK-030`) ist im Entwickler-Report explizit dokumentiert.
  - Build/Static-Checks fuer die betroffenen Environments laufen erfolgreich.

- **verification**:
  - `pio run -e gnss_buildup`
  - `pio run -e gnss_bringup_ntrip`
  - Serial-Smoke-Test: DHCP→statisch→DHCP inkl. Reboot-Sequenzen und Log-Pruefung.
  - Negativtest: ungueltige IPv4-Eingaben (`999.999.1.1`, falsche Maske, leeres Gateway bei statisch) werden abgewiesen.
  - Recovery-Test: absichtliche Fehlkonfiguration setzen und via Serial-Reset auf DHCP/Defaults zurueckfuehren.

- **Links**:
  - `backlog/epics/EPIC-004-feature-expansion.md`
  - `backlog/tasks/TASK-020-tft-espi-und-lilygo-eth-lite-treiber-integration-planen.md`
  - `backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md`
  - `backlog/tasks/TASK-030-task025-backlog-status-und-ntrip-architektur-anpassung.md`
  - `backlog/tasks/TASK-032-ntrip-hwstatus-bug-gnss-falscher-fehler.md`
  - `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
