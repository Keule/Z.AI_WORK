# ADR-NTRIP-002: Multi-Receiver-RTCM-Verteilung mit Peek-then-Pop

- Status: proposed
- Datum: 2026-04-23

## Kontext

Der aktuelle RTCM-Forwarding-Pfad in `src/logic/ntrip.cpp` ist für den Single-Receiver-/Single-Base-Betrieb ausreichend, zeigt aber bei mehreren aktiven GNSS-Empfängern ein relevantes Verlustmuster.

Ist-Zustand (vereinfacht):

1. `ntripReadRtcm()` liest RTCM-Daten vom TCP-Socket in den gemeinsamen Ringpuffer.
2. `ntripForwardRtcm()` iteriert über alle GNSS-Instanzen.
3. Pro Instanz wird mit `rtcmRingPeek()` gelesen und **direkt nach jedem erfolgreichen Write** via `rtcmRingPop()` konsumiert.

Damit gilt aktuell ein **Pop-first-Effekt auf Systemebene**: Der zuerst bediente Empfänger konsumiert Daten aus dem gemeinsamen Puffer, bevor spätere Empfänger dieselben Bytes erhalten können.

## Problem / Risiko

Bei produktiver Multi-Receiver-Nutzung (z. B. Dual-UM980) führt das aktuelle Verhalten zu potenziellem Datenverlust für nachgelagerte Empfänger:

- Empfänger A erhält und konsumiert RTCM-Bytes.
- Empfänger B sieht diese Bytes ggf. nicht mehr, weil sie bereits aus dem Ringpuffer entfernt wurden.
- Bei partiellen Writes oder unterschiedlicher UART-Aufnahmefähigkeit verschärft sich die Asymmetrie.

Folge: inkonsistente Korrekturdatenversorgung zwischen Empfängern und potenziell schlechteres/instabiles RTK-Verhalten auf einzelnen Ports.

## Entscheidung (Zielverhalten)

Für Multi-Receiver-Profile wird ein **distributives Peek-then-Pop-Verhalten** definiert:

1. RTCM-Daten werden aus dem Ringpuffer zunächst nur per Peek bereitgestellt.
2. Dieselben Daten werden an **alle aktiv/ready markierten Ziel-Empfänger** verteilt.
3. Erst nachdem die Verteilung für den betrachteten Datenabschnitt abgeschlossen ist, werden die Bytes im gemeinsamen Puffer **einmalig** via Pop konsumiert.

Kurzregel:

- **Erst verteilen (Peek), dann einmalig konsumieren (Pop).**

## Trigger-Bedingung für Umsetzung

Die technische Umsetzung dieses ADR wird verpflichtend aktiv, **sobald Multi-Receiver-Profile produktiv genutzt werden**.

Bis dahin kann der bestehende Pfad als Single-Receiver-kompatibler Übergang bestehen bleiben, sofern keine produktive Mehrfachverteilung erforderlich ist.

## Konsequenzen

### Positiv

- konsistente RTCM-Versorgung aller aktivierten Empfänger in Multi-Receiver-Profilen
- klare Semantik für gemeinsamen RTCM-Puffer
- bessere Grundlage für Dual-UM980-Failover- und Vergleichsbetrieb

### Negativ

- höhere Komplexität im Forwarding-Pfad (Koordination mehrerer Writer)
- zusätzlicher Testaufwand für partielle Writes/Backpressure
- mögliche Notwendigkeit klarer Regeln für langsamste Instanz (Drop-, Retry- oder Skip-Policy)

## EPIC/TASK-Kontext

Diese ADR konkretisiert den Architekturpfad im Kontext der Dual-UM980-Produktivierung und steht im Zusammenhang mit `TASK-019*`, insbesondere:

- `TASK-019` Konsolidierung / Integrationspfad
- `TASK-019F` Dual-UM980 Failover-Logik
- `TASK-019G` Labor-/Feldvalidierung

Referenzdokument:

- `docs/plans/TASK-019-konsolidierung-2026-04-17.md`

## Alternativen

- getrennte RTCM-Puffer pro Empfänger statt gemeinsamer Quelle  
  → robuster gegenüber Backpressure, aber mehr Speicher/Komplexität.
- unverändertes Pop-first-Verhalten beibehalten  
  → für produktive Multi-Receiver-Profile nicht akzeptabel.

## Plan-Integration

Wird in Schritt 6 (Feature-Config & Diagnose) berücksichtigt bei der GNSS-Konfiguration.

