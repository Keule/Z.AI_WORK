# ADR-STATE-001: Strikter `StateLock`-Schutz für geteilten Runtime-State

- Status: accepted
- Datum: 2026-04-23

## Kontext

Das aktuelle Task-Modell trennt zeitkritische Regelung, Kommunikation und Wartung
auf mehrere Tasks (siehe ADR-002). Dadurch greifen mehrere Tasks auf denselben
Runtime-State zu.

Im Ist-Zustand ist der Schutz nicht konsistent:

- **Write-Pfad von `desiredSteerAngleDeg` in `commTask`/Netzpfad**
  - `src/logic/net.cpp`: Setpoint aus `PGN 254` wird direkt in
    `desiredSteerAngleDeg` geschrieben.
- **Read-Pfade in anderen Tasks**
  - `src/logic/control.cpp`: Regler liest `desiredSteerAngleDeg` für den
    Sollwinkel.
  - `src/logic/sd_logger.cpp`: Logger liest `desiredSteerAngleDeg` für
    Telemetrieaufzeichnung.

Parallel dazu werden andere Teile von `g_nav` bereits mit `StateLock` geschützt.
Die Mischung aus gelockten und ungelockten Zugriffen erschwert Korrektheit,
Code-Reviews und zukünftige Migrationen.

## Entscheidung

Es gilt ab sofort die verbindliche Regel:

1. **Jeder Wert mit Zugriff aus mehr als einem Task MUSS geschützt werden.**
2. Zulässige Schutzmechanismen sind ausschließlich:
   - **`StateLock`** für strukturierten gemeinsamen Zustand, oder
   - **klar begründete Atomics** für eng begrenzte, lock-freie Spezialfälle.
3. **Ungeschützte `extern`-Variablen für Cross-Task-Daten sind nicht zulässig.**
4. Für neue Cross-Task-Felder ist die Standardwahl `NavigationState` +
   `StateLock`.

## Migrationsregel

Für den aktuellen Sonderfall wird verbindlich festgelegt:

- `desiredSteerAngleDeg` wird nach `NavigationState` in
  `src/logic/global_state.h` migriert.
- Lesen und Schreiben dieses Werts erfolgt ausschließlich unter `StateLock`.
- Der bisherige ungeschützte globale Pfad (`extern volatile float`) wird nach
  Migration entfernt.

## Leitplanken zu bestehenden ADRs

Diese Entscheidung konkretisiert bestehende Architekturleitplanken und ist mit
ihnen konsistent:

- **ADR-001** (`ADR-001-config-layering-fw-soft-runtime.md`): klare
  Verantwortlichkeiten und konsistente State-Modelle.
- **ADR-002** (`ADR-002-task-model-control-comm-maint.md`): Cross-Task-Zugriffe
  folgen dem Task-Modell und benötigen explizite Synchronisation.
- **ADR-003** (`ADR-003-feature-module-system-and-pin-claims.md`): explizite,
  überprüfbare Regeln statt impliziter Seiteneffekte.

## Konsequenzen

### Positiv

- deterministischere Cross-Task-Sichtbarkeit von Zustandsänderungen
- weniger Race-Condition-Risiko
- klarere Review-Regeln („shared state => Lock oder begründetes Atomic“)

### Negativ

- zusätzlicher Aufwand bei Migration bestehender Legacy-Zugriffe
- potenziell mehr Lock-Granularitätsdiskussionen in Hotpaths

## Alternativen

- **Beibehaltung der Mischform (teils Lock, teils `volatile`)**
  - verworfen: `volatile` löst keine gegenseitige Exklusion und keine
    konsistente Cross-Task-Synchronisation.
- **Generelles Atomic-only-Modell**
  - verworfen: für strukturierte Zustände (`NavigationState`) schlechter
    wartbar und fehleranfälliger in zusammengesetzten Updates.

## Plan-Integration

Migration von desiredSteerAngleDeg wurde in Schritt 1c umgesetzt. Status wird auf `accepted` geändert.

