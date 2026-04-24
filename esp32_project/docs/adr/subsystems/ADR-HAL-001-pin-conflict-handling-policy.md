# ADR-HAL-001: Harte Politik für Pin- und Ressourcen-Konflikte

- Status: accepted
- Datum: 2026-04-20

## Kontext

Im Projekt treten reale Konflikte zwischen Pins und gemeinsam genutzten
Ressourcen auf. Diskutiert wurde insbesondere, dass erkannte Konflikte
nicht nur kommentiert oder weich akzeptiert werden dürfen.

## Entscheidung

Pin- und exklusive Ressourcen-Konflikte werden standardmäßig **hart**
behandelt:

- Konflikt → Aktivierung oder Initialisierung schlägt fehl,
- Ausnahme nur mit expliziter dokumentierter Übergangsregel.

## Invarianten

- Ein Kommentar im Boardprofil ersetzt keine technische Konfliktbehandlung.
- Ein bereits vorhandener Claim ist nicht automatisch konfliktfrei.
- Legacy- oder Boot-Claims dürfen nur mit dokumentierter Ausnahme übernommen werden.
- Konflikte müssen im Boot-/Diagnoselog sichtbar sein.

## Konsequenzen

### Positiv
- versteckte Fehlkonfigurationen werden früher sichtbar
- Diagnose wird robuster
- Architekturabsicht wird nicht aufgeweicht

### Negativ
- temporär mehr harte Fehler im Übergang von Legacy-Systemen
- höherer Aufwand bei Migration und Test

## Alternativen

- Konflikte nur warnen und weiterlaufen  
  → zu riskant.
- Konflikte stillschweigend überschreiben  
  → nicht akzeptabel.
