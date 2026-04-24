# ADR-004: CRC-Grenze zwischen Core-AOG und Discovery/Management

- Status: accepted
- Datum: 2026-04-20

## Kontext

Discovery-/Management-PGNs und Core-AOG-PGNs folgen praktisch nicht exakt
derselben Strictness. In Diskussionen und vorhandener Doku wurde die Grenze
zwischen tolerantem Discovery-Pfad und striktem Core-Pfad als unverhandelbar
herausgearbeitet.

## Entscheidung

Die Protokollgrenze bleibt explizit getrennt:

- **Core-AOG-Pfad**  
  CRC/Checksum strikt validieren

- **Discovery-/Management-Pfad**  
  definierte Kompatibilitätsausnahme mit tolerantem Verhalten

## Invarianten

- Änderungen an einem Pfad dürfen den anderen Pfad nicht implizit verändern.
- Die Umschaltlogik muss zentral nachvollziehbar bleiben.
- Tasks, die den Protokollpfad berühren, müssen explizit dokumentieren, welcher Pfad betroffen ist.

## Konsequenzen

### Positiv
- höhere Kompatibilität ohne verdeckte Aufweichung des Core-Pfads
- klarere Teststrategie

### Negativ
- zwei semantisch unterschiedliche Pfade müssen bewusst gepflegt werden

## Alternativen

- CRC überall strikt  
  → Discovery-Kompatibilität gefährdet.
- CRC überall tolerant  
  → Core-AOG-Pfad würde unkontrolliert aufgeweicht.
