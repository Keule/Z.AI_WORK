# ADR-003: Feature-Modulsystem mit Dependencies und Pin-Claims

- Status: accepted
- Datum: 2026-04-20

## Kontext

Das Projekt benötigt optional aktivierbare Hardware-/Firmwareteile. Gleichzeitig
gibt es Pin- und Ressourcenüberschneidungen zwischen Modulen und Spezialpfaden.
Ein rein implizites System führt zu verdeckten Konflikten und schwerer Diagnose.

## Entscheidung

Es gibt ein Feature-Modulsystem mit:

- compile-time Verfügbarkeit
- runtime Aktivierbarkeit
- expliziten Dependencies
- expliziten Pin-Gruppen
- Claim-/Conflict-Prüfung für Pins und andere exklusive Ressourcen

## Invarianten

- Dependencies müssen eindeutig kodiert sein; gültige Modul-IDs dürfen nicht gleichzeitig Terminatoren sein.
- Ein Modul darf nicht als „aktiv“ gelten, wenn seine Dependencies nicht erfüllt sind.
- Pin-Konflikte dürfen nicht stillschweigend toleriert werden.
- Bereits vorhandene Legacy-Claims dürfen nur mit explizit dokumentierter Ausnahme akzeptiert werden.

## Konsequenzen

### Positiv
- bessere Sichtbarkeit von Ressourcen- und Aktivierungskonflikten
- sauberere Diagnose
- klarere Trennung von compile-time und runtime Verhalten

### Negativ
- höherer Modellierungsaufwand
- Übergang vom Legacy-System kann temporär doppelte Strukturen erzeugen

## Alternativen

- reine Compile-Time-Features ohne Runtime-Aktivierung  
  → zu starr.
- implizite Aktivierung nur über Codepfade  
  → Konflikte bleiben verborgen.
