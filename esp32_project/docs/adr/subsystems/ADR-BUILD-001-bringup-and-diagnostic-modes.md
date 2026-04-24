# ADR-BUILD-001: Bringup- und Diagnosemodi als explizite Sonderpfade

- Status: accepted
- Datum: 2026-04-20

## Kontext

Für IMU-, GNSS- und andere Hardwarebringup-Szenarien werden reduzierte
Initialisierungspfade benötigt. Diese dürfen die Produktionslogik nicht
verdeckt verändern.

## Entscheidung

Bringup- und Diagnosemodi sind explizite Sonderpfade mit eigener Doku und
klar eingeschränktem Geltungsbereich.

Beispiele:
- GNSS-Buildup
- IMU-Bringup
- andere dedizierte Diagnoseprofile

## Invarianten

- Sonderpfade gelten nur für den jeweils dokumentierten Modus.
- Sonderpfade dürfen globale Architekturentscheidungen nicht stillschweigend brechen.
- Abweichungen vom normalen Taskmodell müssen explizit dokumentiert sein.

## Konsequenzen

### Positiv
- schnellere Diagnose
- geringere Kopplung an Vollinitialisierung

### Negativ
- zusätzlicher Doku- und Testaufwand
- Risiko, dass Sonderpfade veralten, wenn sie nicht gepflegt werden

## Alternativen

- Diagnose nur über Produktionspfad  
  → zu langsam, zu unübersichtlich.
- versteckte Sonderbehandlungen ohne Doku  
  → nicht akzeptabel.
