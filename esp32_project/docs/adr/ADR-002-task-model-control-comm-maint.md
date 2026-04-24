# ADR-002: Taskmodell mit controlTask, commTask und maintTask

- Status: accepted
- Datum: 2026-04-20

## Kontext

Die Firmware hat harte und weiche Laufzeitanforderungen. Sensorik, Regler,
Kommunikation, SD-Flush und Netzwerkverbindungen haben unterschiedliche
Zeitkritikalität. Ohne klare Aufgabenverteilung werden blockierende Operationen
in falsche Kontexte geschoben.

## Entscheidung

Die Laufzeitlogik wird in drei Hauptbereiche getrennt:

- **controlTask**
  - hochfrequente, zeitkritische Regel- und Aktuatorlogik
- **commTask**
  - Kommunikationspolling, PGN-/Netzpfad, nicht-blockierende I/O-Schritte
- **maintTask**
  - blockierende oder potenziell langsame Hintergrundarbeit
  - z. B. Connect/Reconnect, SD-Flush, Monitoring

## Invarianten

- `controlTask` darf keine blockierenden SD- oder Netzwerkoperationen ausführen.
- `commTask` darf keine blockierende Connect-Logik ausführen, außer in explizit dokumentierten Diagnose-/Fallbackmodi.
- `maintTask` darf blockieren, solange die höheren Pfade stabil bleiben.

## Konsequenzen

### Positiv
- bessere zeitliche Vorhersagbarkeit
- klarere Fehlersuche
- Hintergrundarbeit wird entkoppelt

### Negativ
- mehr Cross-Task-Koordination
- Bedarf an sauber definierten Puffern und Statusmodellen

## Alternativen

- alles in einem Haupttask  
  → zu viel Kopplung.
- nur control + comm ohne maintTask  
  → blockierende Hintergründe landen zu leicht im falschen Task.
