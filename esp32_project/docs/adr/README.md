# ADR-Ordner

Dieser Ordner enthält die maßgeblichen **Architecture Decision Records** des Repositories.

## Zweck
ADRs halten Entscheidungen fest, die:
- dauerhaft relevant sind,
- architektonisch oder subsystem-relevant sind,
- später sonst falsch rekonstruiert würden.

## Was **nicht** in ADRs gehört
- allgemeine menschliche Bedienanleitungen,
- reine Agentenrollen,
- triviale Taskschritte,
- bloße Zwischenstände,
- Handover-Inhalte ohne dauerhafte Entscheidungswirkung.

## Ebenen

### Repo-/System-ADRs
Liegen direkt unter `docs/adr/`.

Sie regeln:
- Dokumentenhierarchie,
- Config-Modelle,
- Task-/Laufzeitmodell,
- Modulsystem,
- übergreifende Protokollgrenzen.

### Subsystem-ADRs
Liegen unter `docs/adr/subsystems/`.

Sie regeln:
- GNSS / NTRIP,
- Logging / SD / PSRAM,
- Bringup-/Diagnosemodi,
- Pin-Konfliktpolitik,
- andere klar abgegrenzte Bereiche.

## Status
Zulässige Stati:
- `proposed`
- `accepted`
- `superseded`

## Konfliktregel
Bei Widerspruch gilt:
- neuere gültige ADRs vor älteren,
- spezifischere ADRs vor allgemeineren, sofern sie diese nicht unzulässig brechen,
- README und `agents.md` regeln Nutzung und Rollen,
- ADRs regeln Architektur und technische Invarianten.

## Pflegehinweis
Wenn ein Handover, ein Plan-Dokument oder ein Chat eine dauerhaft relevante technische Entscheidung enthält, sollte diese als ADR konserviert werden.
