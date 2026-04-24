# ADR-000: Dokumentenhierarchie und Konfliktauflösung

- Status: accepted
- Datum: 2026-04-20

## Kontext

Im Repo existieren README, Agentenregeln, Tasks, Handover- und Plan-Dokumente.
Ohne klare Hierarchie entstehen Widersprüche und Folgeagenten können alte oder
halbnormative Texte falsch priorisieren.

## Entscheidung

Für das Repository gilt folgende Hierarchie:

1. `README.md` für menschliche Repo-Nutzung und Zusammenarbeit Mensch ↔ KI
2. `agents.md` für Agentenverhalten und Rollenlogik
3. einschlägige ADRs für Architekturentscheidungen
4. Tasks für konkrete Arbeitsaufträge
5. aktueller Repo-Code als Ist-Zustand
6. Handover- und Plan-Dokumente nur als Kontext

## Invarianten

- Handover- und Plan-Dokumente dürfen keine höher priorisierte Quelle überstimmen.
- Wichtige Chatinhalte müssen in Task, ADR oder Report überführt werden.
- Ein späterer Agent muss ohne Originalchat weiterarbeiten können.

## Konsequenzen

### Positiv
- Widersprüche werden auflösbar.
- Alte Texte bleiben nutzbar, ohne normativ zu dominieren.
- Repo-Wissen wird stabiler.

### Negativ
- Bestehende alte Prozessdokumente müssen ggf. umklassifiziert oder gekürzt werden.
- Pflegeaufwand steigt kurzfristig beim Umbau.

## Alternativen

- Alles im README bündeln  
  → zu groß, zu unpräzise für Architekturentscheidungen.
- Handover als normative Quelle mitlaufen lassen  
  → zu instabil und zu stark vom Verlauf einzelner Sessions abhängig.
