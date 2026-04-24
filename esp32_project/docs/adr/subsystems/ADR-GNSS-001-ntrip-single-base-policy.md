# ADR-GNSS-001: NTRIP-Policy für Single-Base-Betrieb

- Status: accepted
- Datum: 2026-04-20

## Kontext

Für das GNSS-/RTCM-Modell wurde ein Single-Base-NTRIP-Pfad diskutiert.
Dabei standen u. a. GGA-Rückkanal, Reconnect-Verhalten und Integration in
das Taskmodell im Fokus.

## Entscheidung

- NTRIP wird als **Single-Base**-Pfad behandelt.
- Kein verpflichtender GGA-Rückkanal als Standardannahme.
- Konfiguration läuft über Config-Layering und RuntimeConfig.
- Der Connect-/Reconnect-Pfad gehört in den `maintTask`.
- `commTask` verarbeitet nur laufenden Datenfluss, soweit möglich nicht-blockierend.

## Invarianten

- Single-Base-Annahmen dürfen nicht stillschweigend in VRS-/GGA-Logik umkippen.
- Leere produktive Defaults dürfen nicht automatisch durch Demo-Ziele ersetzt werden.
- NTRIP-Status muss mit GNSS-/HW-Status konsistent modelliert werden.

## Konsequenzen

### Positiv
- klarer, reduzierter Integrationspfad
- bessere Laufzeittrennung

### Negativ
- spätere VRS-/GGA-Erweiterungen brauchen explizite neue Entscheidung
- Reconnect-Policy muss bewusst gepflegt werden

## Alternativen

- von Anfang an generischer Multi-Mode-NTRIP-Client  
  → unnötig komplex.
- Connect direkt im commTask  
  → blockierungsanfälliger.
