# ADR-LOG-001: Logging über Pufferung und SD-Flush im Hintergrund

- Status: accepted
- Datum: 2026-04-20

## Kontext

Sensorik und Regler sind zeitkritisch, SD-Zugriffe und Busumschaltungen nicht.
In Diskussionen wurde deshalb eine Entkopplung des Logging-Pfads über Pufferung
und Hintergrund-Flush gefordert.

## Entscheidung

- Der schnelle Pfad schreibt nur in einen Ringbuffer.
- Wenn möglich, wird ein großer Buffer in PSRAM genutzt.
- SD-Schreiben erfolgt in einem niedriger priorisierten Hintergrundkontext.
- Busumschaltungen für SD sind als Übergangslösung akzeptabel, aber nicht im
  hochfrequenten Regelpfad.

## Invarianten

- `controlTask` darf keine direkten SD-Schreiboperationen ausführen.
- Logging muss auch bei temporär nicht verfügbarem SD-Zugriff weiter puffern können.
- Ein reiner Buffer ohne sauber dokumentierten Drain-Pfad gilt nicht als vollständige Lösung.

## Konsequenzen

### Positiv
- geringere Störung des Regelpfads
- bessere Nutzbarkeit von PSRAM
- klarere Entkopplung

### Negativ
- zusätzlicher Speicher- und Konsistenzaufwand
- Ringbuffer- und Overflow-Politik müssen sauber dokumentiert werden

## Alternativen

- direktes SD-Schreiben aus dem Regelpfad  
  → zeitlich zu riskant.
- nur kleiner statischer Buffer ohne Skalierung  
  → schnell überlaufanfällig.
