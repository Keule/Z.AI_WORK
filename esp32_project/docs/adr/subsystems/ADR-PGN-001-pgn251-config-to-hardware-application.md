# ADR-PGN-001: PGN-251-Konfigurationsbits zur Hardwareanwendung

- Status: accepted
- Datum: 2026-04-23

## Kontext

`STEER_CONFIG_IN` (PGN 251) wird in `src/logic/net.cpp` empfangen und dekodiert.
Der aktuelle Pfad persistiert die Rohwerte in `g_nav.pid` und wendet derzeit nur einen
Teil der Bits unmittelbar an (`set0 bit4` als Motor-Treiberauswahl Cytron/IBT2 über
`RuntimeConfig.actuator_type`).

Für einen robusten Betrieb muss klar getrennt werden, welche Bits sicher zur Laufzeit
übernommen werden können und welche nur über einen kontrollierten Reinit-/Neustart-Pfad
wirksam werden dürfen.

## Ist-Zustand (Codepfad)

- Empfang/Decode von PGN 251 in `netDispatchAogPgn(...)`.
- Spiegelung nach `g_nav.pid.config_*` als beobachtbarer Runtime-Status.
- `applySteerConfigBits(...)` loggt alle bekannten `set0`-Bits.
- Funktional angewendet wird aktuell nur:
  - `CONFIG_BIT_DRIVER_CYTRON` (`set0 bit4`) → `RuntimeConfig.actuator_type`.
- Die übrigen Bits sind noch nicht funktional bis in HAL-/Actuator-Initialisierung
  durchverdrahtet.

## Entscheidung

Wir führen ein zweistufiges Anwendungsmodell für PGN-251-Bits ein:

### Stufe A – laufzeitfähige Bits ohne Neustart

Bits in dieser Stufe dürfen im Betrieb übernommen werden, wenn sie keine
zeitkritischen Pfade unterbrechen und ohne Hardware-Reinit wirksam werden können.

- Beispielkandidaten (nach technischer Verifikation):
  - logische Invertierungen oder softwareseitige Richtungs-/Polarity-Flags,
  - nicht-blockierende Schalter für Auswertepfade.
- Anwendung erfolgt entkoppelt vom zeitkritischen Loop (z. B. via kontrolliertem
  Apply-Point im Maint-/Konfigpfad).

### Stufe B – Bits mit Reinit-/Neustartbedarf

Bits in dieser Stufe verändern Hardware-/Treibergrundannahmen und benötigen
expliziten Reinit oder Neustart.

- Beispielkandidaten:
  - Treiberklasse (`Cytron` vs. `IBT2`),
  - Pin-/Signalmoduswechsel,
  - Änderungen, die HAL-Handles oder Initialisierungsreihenfolge betreffen.
- Eingehende Werte werden angenommen und dokumentiert, aber erst über einen
  kontrollierten Reinit-/Neustartprozess aktiv geschaltet.

## Sicherheitsregeln

- **Kein abruptes Reconfig im zeitkritischen Pfad** (`commTask`, Regel-/ISR-nahe Abschnitte).
- Keine blockierenden, hardwareseitigen Reinitialisierungen direkt in der PGN-Dispatch-Verarbeitung.
- Jede Stufe-B-Änderung braucht expliziten Zustandsübergang (pending → apply) mit
  nachvollziehbarem Log/Diagnoseeintrag.
- Bei unklarer Einordnung eines Bits gilt konservativ Stufe B.

## Konsequenzen

### Positiv
- Reduziertes Laufzeitrisiko bei Konfigurationsänderungen über Netzwerk.
- Klare Semantik für zukünftige Implementierung weiterer `set0`-Bits.
- Bessere Testbarkeit durch getrennte Apply-Pfade.

### Negativ
- Höherer Implementierungsaufwand (Pending-State, Reinit-Orchestrierung).
- Verzögerte Wirksamkeit für Stufe-B-Bits bis zum nächsten kontrollierten Apply-Point.

## Backlog-Kontext

- `backlog/tasks/TASK-003-pgn251-config-anwenden.md`

## Plan-Integration

Wird in Schritt 5 (Basis-Konfig-Framework) bei der Config-Kategorie-Implementierung berücksichtigt.
