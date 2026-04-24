# TASK-043 Parametrisierung beider UM980-UARTs planen

- **ID:** TASK-043
- **Titel:** Planungs-Task für die Parametrisierung beider UM980-UARTs erstellen
- **Status:** open
- **Priorität:** high
- **Komponenten:** `src/logic/runtime_config.*`, `src/logic/modules.*`, `src/hal_esp32/hal_impl.cpp`, `src/main.cpp`, UART-Rollenmodell UM980-A/B
- **Owner:** ki-planer
- **Dependencies:** TASK-037, TASK-038, TASK-019A, TASK-019C, TASK-019D, TASK-014, TASK-017
- **delivery_mode:** firmware_only
- **task_category:** feature_expansion
- **Epic:** EPIC-004
- **base_branch:** task/TASK-019
- **Origin:** User-Request (Chat, 2026-04-21) — Parametrisierung beider UM980-UARTs mit Rollenzuordnung, Kollisionsprüfung und sicherer Persistenz.

## Pflichtsatz für Ausführung

Vor Ausführung dieses Tasks müssen `README.md` und `agents.md` gelesen und befolgt werden.

## Kontext / Problem

Die aktuelle Dual-UM980-Integration hat Bringup-, Spiegel- und Failover-Bausteine, aber noch keinen dedizierten Umsetzungsauftrag für eine **separate, persistente und konfliktgeprüfte UART-A/B-Parametrisierung** inklusive Rollenmodell je UART. Für robuste Feldkonfiguration müssen UART-Parameter, Rollen und Validierungs-/Fallbackregeln als zusammenhängender Entwicklungsauftrag vorliegen.

## Scope (in)

1. **UART-A/B Parameter pro Port**
   - Baudrate, Parität, Stopbits
   - RX-/TX-Pin-Zuweisung
   - optional Hardware-Flow-Control (RTS/CTS), falls Board/Profil dies unterstützt
2. **Rollenmodell je UART**
   - konfigurierbare Rollen je Port, z. B. NMEA out, RTCM in/out, Diagnostik/Mirror
   - definierte erlaubte/unerlaubte Rollenkombinationen
3. **Konflikterkennung und UX-Hinweise**
   - Erkennung von Pin-Kollisionen und Rollenkollisionen
   - klare Nutzer-Rückmeldung (Log/API/UI-Pfad im bestehenden Konfigurationskanal)
4. **Persistenz + sicherer Fallback**
   - Konfigurationsspeicherung für beide UARTs
   - Start mit validen Defaults, wenn gespeicherte Konfiguration ungültig/nicht initialisierbar ist
5. **Abhängigkeits- und Schnittstellenklärung**
   - Abgleich mit bestehendem Dual-UM980-/RTCM-Task-Stack
   - konkrete Benennung der zu ändernden öffentlichen Schnittstellen

## Nicht-Scope (out)

- Vollständige Implementierung der Dual-UM980-Failover-Strategie selbst (TASK-019F; nutzt Ergebnisse dieses Planungs-Tasks)
- Labor-/Feld-Endvalidierung (TASK-019G)
- Neue Hardware-Revisionen oder Board-Redesign

## Akzeptanzkriterien (AC)

1. Beide UARTs sind **separat** konfigurierbar (Parameter + Rolle je Port).
2. Konflikte bei Pin- oder Rollenkollision werden erkannt und dem Nutzer angezeigt.
3. Persistenz ist implementiert, inklusive sicherem Fallback auf valide Defaults.
4. Abhängigkeiten zu bestehenden Dual-UM980-/RTCM-Tasks (`TASK-019*`, `TASK-014`, `TASK-017`) sind nachvollziehbar dokumentiert.
5. Invarianten sind explizit dokumentiert (keine ungültigen Pin-Kombinationen; kein Start mit nicht initialisierbarer UART-Konfiguration ohne Fallback).
6. `merge_risk_files` und erwartete Schnittstellenänderungen sind explizit benannt.

## Abhängigkeiten (geprüft)

- **TASK-019A**: Legt Pinbelegungsgrundlagen für UM980/UART fest; bildet physische Basis für UART-A/B-Konfigurierbarkeit.
- **TASK-019C**: Bringup-/Init-Modus beeinflusst, wann UART-Konfigurationen sicher angewendet werden können.
- **TASK-019D**: UART-Mirror/Diagnostik ist direkte Rollen-Option und kollidiert potenziell mit produktiven Datenrollen.
- **TASK-014**: RTCM-UART-Forwarding definiert den bestehenden RTCM-Datenpfad und muss im Rollenmodell je UART konsistent bleiben.
- **TASK-017**: Validierungskriterien für RTCM-Ende-zu-Ende liefern Messlatte für neue UART-Rollenbelegung.
- **TASK-019F**: Baut fachlich auf den hier definierten UART-Rollen-, Validierungs- und Fallbackregeln auf (Dependency-Richtung: `TASK-019F -> TASK-043`).

## Invarianten

- Keine ungültigen oder doppelt belegten RX/TX-Pin-Kombinationen im aktiven Setup.
- Rollenmodell erlaubt keine gleichzeitigen, logisch widersprüchlichen Portrollen ohne explizit definierte Priorisierung.
- Systemstart erfolgt niemals mit nicht initialisierbarer UART-Konfiguration; stattdessen erzwungener Fallback auf valide Defaults mit Diagnosehinweis.
- Persistierte Konfiguration wird vor Anwendung vollständig validiert (Schema + Hardware-/Pin-Kompatibilität).

## Known Traps

- UART-Mirror (Diagnostik) kann produktive Bandbreite beeinflussen und darf nicht stillschweigend RTCM/NMEA-Rollen degradieren.
- Unterschiedliche Board-Profile können Flow-Control-Pins nur teilweise unterstützen; Capability-Gating erforderlich.
- Failover-Logik kann fehldeuten, wenn Rollen- oder Baudratenwechsel ohne saubere Re-Init-Sequenz erfolgen.

## Abgrenzung zu TASK-041 (generische GNSS-Output-Policy)

- **TASK-043 verantwortet ausschließlich UM980-UART-A/B-spezifische Parametrisierung**:
  - konkrete Pins (`tx_pin`, `rx_pin`, optional `rts_pin`/`cts_pin`),
  - konkrete Portrollen (`NMEA out`, `RTCM in/out`, `Diagnostik/Mirror`, `disabled`),
  - Pin-/Rollenkollisionserkennung,
  - UART-spezifischer Fallback bei ungültiger oder nicht initialisierbarer Konfiguration.
- **TASK-043 verantwortet nicht**:
  - generische Datentyp-Policy über alle GNSS-Zielpfade,
  - globale Output-Rate/Profile-Definitionen für GNSS-Outputs,
  - übergreifende Policy-Regeln für nicht-UART-Zielpfade.
- Diese generischen Policy-Aspekte sind Scope von `TASK-041`.

## Explizite Schnittstelle: Lieferung von TASK-043 an TASK-041

`TASK-043` liefert die von `TASK-041` konsumierte UART-Schnittstelle (Provider/Consumer-Vertrag):

1. `enum Um980UartPortId { UART_A, UART_B }`
2. `enum Um980UartRole { ROLE_NMEA_OUT, ROLE_RTCM_IN, ROLE_RTCM_OUT, ROLE_DIAG_MIRROR, ROLE_DISABLED }`
3. `enum Um980UartConfigStatus { OK, PIN_CONFLICT, ROLE_CONFLICT, UNSUPPORTED_FLOW_CONTROL, FALLBACK_APPLIED }`
4. `struct Um980UartPortConfig { port_id, baudrate, parity, stopbits, tx_pin, rx_pin, rts_pin?, cts_pin?, role, enabled }`
5. `struct Um980UartResolvedConfig { uart_a, uart_b, status, fallback_reason? }`

**Regel:** Änderungen an diesen UART-spezifischen Feldern/Enums erfolgen in `TASK-043` (oder expliziter Folgearbeit dazu). `TASK-041` konsumiert diese Daten read-only zur Zielpfad-/Profil-Policy und führt keine eigene Hardware-Rollenmodellierung ein.

## Rejected Alternatives

- **Nur ein globales UART-Profil für beide UM980**: verworfen, da keine saubere Trennung von Primär/Sekundär-Rolle und geringer Diagnosewert.
- **Keine Persistenz, nur Boot-Defaults**: verworfen, da nicht praxistauglich für Feldbetrieb und wiederholbare Deployments.
- **Kollisionen nur loggen, aber akzeptieren**: verworfen, da gegen Sicherheits-/Stabilitätsinvariante.

## files_read

- `README.md`
- `agents.md`
- `backlog/index.yaml`
- `backlog/tasks/TASK-019-integrationsplanung-zwei-um980.md`
- `backlog/tasks/TASK-019A-pinbelegung-um980-und-konsole.md`
- `backlog/tasks/TASK-019C-gnss-bringup-modus.md`
- `backlog/tasks/TASK-019D-uart1-uart2-console-mirror.md`
- `backlog/tasks/TASK-019F-dual-um980-failover-logik.md`
- `backlog/tasks/TASK-014-hal-gnss-rtcm-uart-forwarding.md`
- `backlog/tasks/TASK-017-rtcm-validierung-agiou-m980.md`

## files_write

- `backlog/tasks/TASK-043-planungs-task-parametrisierung-beider-um980-uarts.md`
- `backlog/index.yaml`

## public_surface (erwartet)

- GNSS/UART-Konfigurationsschema (z. B. `RuntimeConfig`/`soft_config`-Strukturen)
- Modul-/HAL-Interfaces zur UART-Initialisierung und Re-Initialisierung
- Nutzerseitige Konfigurationsschnittstelle (PGN/CLI/Web/Datei — abhängig von bestehendem Pfad)

## Erwartete Schnittstellenänderungen

1. Erweiterung der GNSS-Konfigurationsdatenstruktur um `uart_a` und `uart_b` Parameterblöcke.
2. Validierungs-API für UART-Konfiguration mit Ergebniscodes (ok, pin_conflict, role_conflict, unsupported_flow_control, fallback_applied).
3. HAL-Initialisierungspfad mit explizitem Apply/Reject/Fallback-Rückkanal für UART-Konfigurationen.
4. Diagnostik-/Statusausgabe ergänzt um aktive UART-Rollen und Fallback-Grund.

## merge_risk_files

- `src/logic/runtime_config.*`
- `src/logic/modules.*`
- `src/hal_esp32/hal_impl.cpp`
- `src/hal_esp32/hal_esp32.*`
- `src/main.cpp`
- `include/hardware_pins.h`
- `backlog/index.yaml`

## Classification

- **classification:** feature_expansion
- **exclusive_before:** TASK-019F (Failover-Implementierung) — erst nach abgeschlossener Parametrisierungs-/Validierungsplanung
- **parallelizable_after:** TASK-019F, TASK-019G sowie Doku-/Report-Tasks zum UART-Rollenmodell

## Relevante ADRs

- Noch kein expliziter neuer ADR erforderlich, sofern nur task-lokale Parametrisierungsdetails umgesetzt werden.
- Falls neue globale Konfliktauflösungsstrategie für Rollen/Pins eingeführt wird, ADR-Update erforderlich.

## Links

- `backlog/tasks/TASK-037-basis-konfig-framework-rahmen-vor-feature-tasks.md`
- `backlog/tasks/TASK-038-boot-safety-pin-serial-konfigmodus.md`
- `backlog/tasks/TASK-019-integrationsplanung-zwei-um980.md`
- `backlog/tasks/TASK-019A-pinbelegung-um980-und-konsole.md`
- `backlog/tasks/TASK-019C-gnss-bringup-modus.md`
- `backlog/tasks/TASK-019D-uart1-uart2-console-mirror.md`
- `backlog/tasks/TASK-014-hal-gnss-rtcm-uart-forwarding.md`
- `backlog/tasks/TASK-017-rtcm-validierung-agiou-m980.md`
- `docs/adr/ADR-003-feature-module-system-and-pin-claims.md`
