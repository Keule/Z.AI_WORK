# TASK: Separater RTCM-UDP Receive-Pfad (AOG getrennt)

## Ziel
- AOG-PGN-Verarbeitung und RTCM-Bytestream strikt trennen, damit RTCM-Pakete **nicht** am AOG-Preamble-/CRC-Pfad verloren gehen.
- RTCM-Daten nicht-blockierend puffern und robust an den GNSS-Transport (`hal_gnss_rtcm_write`) weiterleiten.

## Port-Spezifikation
- **AOG Steuer-/PGN-Pfad**
  - `UDP 8888` (Listen): AgIO sendet AOG-Frames an das Modul.
  - `UDP 9999` (Send): Modul sendet AOG-Frames an AgIO.
  - `UDP 5126` (lokaler Source-Port): Steer-Modul-Quellport.
- **RTCM-Pfad (neu, separater Socket)**
  - `UDP 2233` (Listen): AgIO/NTRIP-Bridge sendet rohe RTCM-Bytes an das Modul.
  - Keine AOG-Preamble-/CRC-Validierung auf diesem Pfad.

## Interaktion mit AgIO
- AgIO bleibt Gateway zwischen AOG und Netzwerk.
- Für Korrekturdaten (NTRIP/RTCM) muss AgIO so konfiguriert werden, dass der RTCM-Bytestream zum Modul auf `UDP 2233` gesendet wird.
- Der Modul-Firmwarepfad behandelt diese Bytes transparent und leitet sie an die GNSS-HAL weiter.

## Firewall-/Netzwerkhinweise
- Eingehend auf dem Modul zulassen:
  - `UDP 8888` (AOG)
  - `UDP 2233` (RTCM)
- Ausgehend vom Modul zulassen:
  - `UDP 9999` (AOG an AgIO)
- Broadcast-/Subnetzregeln (AgIO Discovery/Subnet-Change) bleiben unverändert.

## Laufzeitverhalten RTCM
- RTCM-UDP wird in einen Ringpuffer geschrieben.
- Bei Überlauf: kontrolliertes Verwerfen mit Zählern (verlorene Pakete/Bytes).
- Forwarding erfolgt nicht-blockierend per `hal_gnss_rtcm_write(...)`.
- Teilwrites werden erkannt; Rest bleibt im Puffer bis zur nächsten Pollrunde.

## Telemetrie (für spätere PGN/Diagnose)
- `rx_bytes`: empfangene RTCM-Bytes.
- `dropped_packets`: Anzahl verworfener RTCM-Datagramme (wegen Puffergrenze).
- `last_activity_ms`: letzte RTCM-Empfangsaktivität (Millis seit Boot).
- Zusätzlich intern: `forwarded_bytes`, `partial_writes`, `overflow_bytes`.
