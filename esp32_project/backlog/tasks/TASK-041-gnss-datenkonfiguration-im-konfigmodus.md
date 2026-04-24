# TASK-041 GNSS-Datenkonfiguration im Konfigmodus spezifizieren

- **ID**: TASK-041
- **Titel**: Konfigurierbare GNSS-Datenausgabe (Typen, Raten/Profile, Zielpfade) im Konfigmodus spezifizieren
- **Status**: open
- **Priorität**: high
- **Komponenten**: `src/logic/runtime_config.*`, `src/logic/modules.*`, GNSS-/Bridge-Datenpfade (`PGN`, `NMEA`, `RTCM`), Konfigmodus/Serial-Web-Konfig, Persistenz (NVS/Datei)
- **Dependencies**: TASK-037, TASK-038, TASK-009, TASK-016, TASK-017, TASK-019, TASK-043
- **delivery_mode**: mixed
- **task_category**: feature_expansion
- **Owner**: ki-planer
- **Epic**: EPIC-004

- **classification**: dependent
- **exclusive_before**: []
- **parallelizable_after**: []

- **Origin**:
  Nutzeranforderung aus Chat (2026-04-21):
  Ein neuer Task soll die GNSS-Datenkonfiguration über den Konfigmodus spezifizieren, inkl.
  Datentypen, Update-Raten/Output-Profile, Zielpfade, ACs, Abgleich mit bestehenden GNSS/PGN-/Bridge-Tasks,
  Known Traps und ADR-Bezug bei Protokoll-/Subsystemgrenzen.

- **Diskussion**:
  - Direkt: https://chatgpt.com/codex/cloud/tasks

- **Kontext/Problem**:
  Der aktuelle Stand enthält mehrere Teilpfade für GNSS-nahe Ausgabe (u. a. PGN/Bridge/RTCM/NMEA),
  aber keine zusammenhängende, explizit spezifizierte Konfigmodus-Definition für:
  - welche Datentypen pro Ziel aktiviert werden dürfen,
  - welche Raten/Profile zulässig sind,
  - wie Persistenz und Reboot-Reproduzierbarkeit garantiert werden,
  - wie inkompatible Kombinationen behandelt werden.
  Ohne diese Spezifikation drohen inkonsistente Konfigurationen, unklare UX im Konfigmodus und
  schwer reproduzierbares Verhalten in Feldtests.

- **Scope (in)**:
  - Verbindliche Konfigmodus-Spezifikation für GNSS-Datenausgabe erstellen mit folgenden Inhalten:
    1. **Konfigurierbare Datentypen** gemäß aktueller Implementierung (mindestens Abgleich zu NMEA, RTCM, PGN; proprietäre Datentypen nur falls im Ist-Zustand vorhanden).
    2. **Update-Raten/Output-Profile** definieren (z. B. low/normal/high oder konkrete Hz/Intervallwerte) inkl. zulässiger Grenzen.
    3. **Zielpfade** pro Datentyp spezifizieren (UART/Netzwerk inkl. Bridge-Kontext).
    4. **Validierungsregeln** für inkompatible Kombinationen (harte Sperre oder klare Warnung mit eindeutiger Diagnose).
    5. **Persistenzpfad** und Reboot-Verhalten (Lesen/Ändern/Speichern/Wiederherstellen) dokumentieren.
  - Task muss explizit festhalten, dass vor Umsetzung `README.md` und `agents.md` gelesen und eingehalten werden müssen.
  - Abgleich der Spezifikation mit vorhandenen Aufgaben:
    - `TASK-007` (GPS-Bridge),
    - `TASK-009` (PGN100/NMEA-Out),
    - `TASK-016` (PGN214 FixQuality/Age),
    - `TASK-017` (RTCM E2E-Validierung),
    - `TASK-019*` (Dual-UM980/GNSS-Buildup/Failover).
  - Falls Protokoll- oder Subsystemgrenzen verändert werden, muss ADR-Referenz oder ADR-Bedarf explizit aufgenommen werden.

- **Nicht-Scope (out)**:
  - Kein vollständiger Implementierungsumbau aller GNSS-Pfade in diesem Task.
  - Keine hardwareseitige Revalidierung aller bisherigen RTCM-/GNSS-Tests.
  - Keine neue UI-Implementierung außerhalb der erforderlichen Spezifikations-/Planungsartefakte.

- **Pflichtlektüre vor Umsetzung**:
  1. `README.md`
  2. `agents.md`
  3. `docs/adr/ADR-001-config-layering-and-override-policy.md` (falls vorhanden und einschlägig)
  4. `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
  5. `docs/adr/subsystems/*` (GNSS/RTCM/Logging-relevante Entscheidungen)
  6. dieser Task (`TASK-041`)

- **files_read**:
  - `README.md`
  - `agents.md`
  - `backlog/index.yaml`
  - `backlog/tasks/TASK-007-gps-bridge-firmware.md`
  - `backlog/tasks/TASK-009-nmea-output-pgn100.md`
  - `backlog/tasks/TASK-016-pgn214-fixquality-age-integration.md`
  - `backlog/tasks/TASK-017-rtcm-validierung-agiou-m980.md`
  - `backlog/tasks/TASK-019-integrationsplanung-zwei-um980.md`
  - `backlog/tasks/TASK-019A-pinbelegung-um980-und-konsole.md`
  - `backlog/tasks/TASK-019B-platformio-environment-gnss-buildup.md`
  - `backlog/tasks/TASK-019C-gnss-bringup-modus.md`
  - `backlog/tasks/TASK-019D-uart1-uart2-console-mirror.md`
  - `backlog/tasks/TASK-019E-smoke-test-reportstandard.md`
  - `backlog/tasks/TASK-019F-dual-um980-failover-logik.md`
  - `backlog/tasks/TASK-019G-dual-um980-labor-feldvalidierung.md`
  - relevante GNSS/PGN/Bridge-Implementierungsstellen unter `src/`

- **files_write**:
  - `backlog/tasks/TASK-041-gnss-datenkonfiguration-im-konfigmodus.md`
  - `backlog/index.yaml`
  - optional: ADR-Datei unter `docs/adr/` oder ADR-Task, falls Boundary-Änderung identifiziert wird

- **public_surface**:
  - Konfigmodus-Verhalten für GNSS-Ausgabeprofile (Nutzer- und Integrationssicht)
  - Persistiertes Konfigschema für GNSS-Output (falls verändert)
  - Mögliche Protokollgrenzen zwischen GNSS-Quelle, PGN-Encoder und Bridge-Ausgabe

- **merge_risk_files**:
  - `src/main.cpp`
  - `src/logic/runtime_config.*`
  - `src/logic/modules.*`
  - GNSS-/Bridge-/PGN-Pfade unter `src/logic/` und `src/hal_esp32/`

- **risk_notes**:
  - Zu aggressive Output-Raten können UART/UDP-Bandbreite überlasten.
  - Mismatch zwischen Profilname und realer Frequenz führt zu schwer reproduzierbaren Fehlern.
  - Persistenz ohne strikte Validierung kann invalide Zustände über Reboots konservieren.

- **Invarianten**:
  - Konfigmodus darf nur wohldefinierte GNSS-Datenkombinationen zulassen oder eindeutig warnen.
  - Nach `Speichern` muss ein Reboot denselben aktiven Satz an Datentypen/Raten/Zielen reproduzieren.
  - Sicherheitskritische Betriebsarten (Autosteer-relevante Datenpfade) dürfen nicht durch versteckte Nebenwirkungen destabilisiert werden.

- **Known traps**:
  - Zu hohe kombinierte Datenraten (z. B. hochfrequentes NMEA + RTCM + Mirror/Bridge) können
    Buffer-Überläufe, Jitter und erhöhte Latenz verursachen.
  - Serielle und Netzwerkausgabe konkurrieren um CPU-Zeit; unpriorisierte Sendewege können
    zeitkritische Frames verdrängen.
  - Fehlkonfiguration kann indirekt Autosteer-Verhalten verschlechtern (veraltete/ausgedünnte
    Position/Fix-Informationen im Regelpfad).
  - Uneinheitliche Definition von „Profil aktiv“ vs. tatsächlich laufender Pipeline führt zu falscher Diagnose.
  - Dual-GNSS-/Failover-Pfade (`TASK-019*`) erhöhen Kombinatorik; Konfigmodus muss Primär/Sekundär- und Fallback-Semantik klar abgrenzen.

- **Abgrenzung zu TASK-043 (UM980 UART-A/B)**:
  - **TASK-041 verantwortet ausschließlich die generische GNSS-Output-Policy**:
    - Datentyp-Klassen (z. B. `NMEA`, `RTCM`, `PGN`),
    - Output-Rate/Profile und zulässige Grenzen,
    - Zielpfad-Klassen (z. B. UART vs. Netzwerk/Bridge),
    - policy-seitige Validierungsregeln (zulässig/verboten auf Policy-Ebene).
  - **TASK-041 verantwortet nicht**:
    - konkrete UART-A/B-Pins,
    - konkrete Portrollen je UM980-UART,
    - Pin-/Ressourcenkollisionen auf Hardwareebene,
    - hardwarenahen Fallback bei UART-Initialisierungsfehlern.
  - Diese UM980-UART-A/B-spezifischen Punkte sind ausschließlich Scope von `TASK-043`.

- **Explizite Schnittstelle zu TASK-043 (von TASK-041 nur konsumiert)**:
  - `TASK-043` liefert eine stabile, konsumierbare UART-Portbeschreibung inkl.:
    - `enum Um980UartPortId { UART_A, UART_B }`
    - `enum Um980UartRole { ROLE_NMEA_OUT, ROLE_RTCM_IN, ROLE_RTCM_OUT, ROLE_DIAG_MIRROR, ROLE_DISABLED }`
    - `enum Um980UartConfigStatus { OK, PIN_CONFLICT, ROLE_CONFLICT, UNSUPPORTED_FLOW_CONTROL, FALLBACK_APPLIED }`
    - `struct Um980UartPortConfig { port_id, baudrate, parity, stopbits, tx_pin, rx_pin, rts_pin?, cts_pin?, role, enabled }`
    - `struct Um980UartResolvedConfig { uart_a, uart_b, status, fallback_reason? }`
  - `TASK-041` nutzt diese Daten **read-only** zur Policy-Entscheidung:
    - Zielpfad-Verfügbarkeit pro Datentyp,
    - zulässige Output-Profile abhängig von aktiver Portrolle/Status,
    - konsistente User-Diagnose im Konfigmodus.
  - Änderungsrecht für obige UART-spezifische Enums/Felder liegt bei `TASK-043`; `TASK-041` darf nur konsumierende Zuordnungstabellen/Policy-Regeln anpassen.

- **Rejected alternatives**:
  - „Freie“ Kombination aller Datentypen ohne Policy:
    - verworfen, weil Überlastung und inkonsistente Zustände wahrscheinlich sind.
  - Nur implizite Dokumentation im Code:
    - verworfen, weil Konfigmodus-Regeln für Nutzer/Tester explizit und prüfbar sein müssen.

- **AC**:
  - Nutzer kann im Konfigmodus mindestens GNSS-Output-Profile **lesen, ändern, speichern**.
  - Gespeicherte GNSS-Konfiguration wirkt nach Reboot **reproduzierbar** (gleiche aktive Datentypen/Raten/Ziele).
  - Inkompatible Konfigkombinationen werden **verhindert** oder mit klarer, nachvollziehbarer Warnung behandelt.
  - Spezifikation enthält expliziten Abgleich mit `TASK-007`, `TASK-009`, `TASK-016`, `TASK-017`, `TASK-019*`.
  - Known-traps-Abschnitt ist vorhanden und enthält mindestens Datenraten-/Buffer-/Latenz- und
    Autosteer-Fehlkonfigurationsrisiken.
  - Bei Änderung von Protokoll-/Subsystemgrenzen ist ADR-Referenz gesetzt oder ADR-Bedarf explizit als Folgearbeit ausgewiesen.

- **verification**:
  - `rg "TASK-041|GNSS-Datenkonfiguration|Konfigmodus" backlog/tasks backlog/index.yaml`
  - Review-Checkliste gegen AC-Liste und Abgleich-Liste `TASK-007/009/016/017/019*`
  - (Falls Implementierungsinkrement startet) Build-/Smoke-Checks gemäß Folgetask

- **Links**:
  - `backlog/tasks/TASK-043-planungs-task-parametrisierung-beider-um980-uarts.md`
  - `backlog/tasks/TASK-007-gps-bridge-firmware.md`
  - `backlog/tasks/TASK-009-nmea-output-pgn100.md`
  - `backlog/tasks/TASK-016-pgn214-fixquality-age-integration.md`
  - `backlog/tasks/TASK-017-rtcm-validierung-agiou-m980.md`
  - `backlog/tasks/TASK-019-integrationsplanung-zwei-um980.md`
  - `backlog/tasks/TASK-019A-pinbelegung-um980-und-konsole.md`
  - `backlog/tasks/TASK-019B-platformio-environment-gnss-buildup.md`
  - `backlog/tasks/TASK-019C-gnss-bringup-modus.md`
  - `backlog/tasks/TASK-019D-uart1-uart2-console-mirror.md`
  - `backlog/tasks/TASK-019E-smoke-test-reportstandard.md`
  - `backlog/tasks/TASK-019F-dual-um980-failover-logik.md`
  - `backlog/tasks/TASK-019G-dual-um980-labor-feldvalidierung.md`
