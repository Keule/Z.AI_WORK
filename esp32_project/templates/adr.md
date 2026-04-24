# ADR Template

## Zweck
Architectural Decision Record für dauerhaft relevante Architekturentscheidungen.

## Minimalpflichtfelder
- **ADR-ID/Titel:**
- **Status:** `proposed | accepted | superseded`
- **Datum:**
- **Kontext:**
- **Entscheidung:**
- **Konsequenzen (positiv/negativ):**
- **Alternativen:**

## Vorlage
```md
# ADR-XXX: <Titel>
- Status: proposed
- Datum: YYYY-MM-DD

## Kontext

## Entscheidung

## Konsequenzen
- Positiv:
- Negativ:

## Alternativen
- A:
- B:
```

## Beispiel
```md
# ADR-002: Zentrale CRC-Boundary bleibt in `pgnIsDiscovery(...)`
- Status: accepted
- Datum: 2026-04-17

## Kontext
Discovery-Management und Core-AOG benötigen unterschiedliche CRC-Toleranz.

## Entscheidung
Die Umschaltung erfolgt ausschließlich über `pgnIsDiscovery(...)` als zentrale Boundary.

## Konsequenzen
- Positiv: Konsistente Entscheidungslogik, weniger implizite Seiteneffekte.
- Negativ: Anpassungen müssen beide Pfade und Tests berücksichtigen.

## Alternativen
- A: CRC-Entscheidung verteilt in jedem Handler.
- B: Compile-Time Feature-Flags je PGN.
```
