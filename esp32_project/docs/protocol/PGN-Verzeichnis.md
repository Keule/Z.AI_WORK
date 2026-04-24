# PGN-Verzeichnis (für Menschen)

Dieses Verzeichnis fasst die im AgOpenGPS-Ökosystem gefundenen PGN-Strukturen zusammen.

## Grundformat (AOG-UDP-PGN)

Alle AOG-Pakete verwenden dieses Grundschema:

- Byte 0: `0x80` (Header)
- Byte 1: `0x81` (Header)
- Byte 2: Source-Adresse (häufig `0x7F`, bei AgIO-NMEA z. B. `0x7C`)
- Byte 3: PGN-ID
- Byte 4: Payload-Länge in Bytes
- Byte 5..N-1: Payload
- Byte N: CRC (`sum(bytes[2..N-1]) mod 256`)

**Endianess-Regel:**
- Mehrbytewerte sind in der Praxis überwiegend **Little Endian (LSB zuerst)** kodiert.
- Erkennbar an `BitConverter.ToInt16/ToUInt16/ToSingle/ToDouble` und Lo/Hi-Byte-Paaren.

## Source-Verzeichnis (verwendete Quellen/IDs)

| Source-ID | Hex | Klarname | Typische Rolle |
|---:|---:|---|---|
| 127 | `0x7F` | AOG Application Source | Standard-Source in AOG-PGN-Frames |
| 124 | `0x7C` | AgIO NMEA Translator Source | GPS/NMEA-Forwarding (D6) |
| 126 | `0x7E` | AutoSteer Modul | Hello/Scan und Modulantworten |
| 123 | `0x7B` | Machine Modul | Hello/Scan und Maschinenstatus |
| 121 | `0x79` | IMU Modul | Hello/Scan |
| 120 | `0x78` | GPS Modul | Scan Reply Typkennung |

## Übliche IPs/UDP-Ports (Transportmatrix)

| Teilnehmer | Übliche IP | UDP Port (Listen) | UDP Port (Send) | Hinweis |
|---|---|---|---|---|
| AgOpenGPS (AOG) | `127.0.0.1` | `15555` | an AgIO `17777` (Broadcast `127.255.255.255`) | interner Loopback |
| AgIO (Loopback) | `127.0.0.1` | `17777` | an AOG `15555` | Bridge zwischen App und Netzwerk |
| AgIO (Netzwerk) | `0.0.0.0` | `9999` | Broadcast `x.x.x.255:8888` | Modulnetz |
| AutoSteer/Machine/IMU/GPS Module | modulabhängig | i. d. R. `8888` | meist an AgIO `:9999` | über UDP-Broadcast/Reply |

## PGN ↔ typische IP/Port-Routen

| PGN/ID | Richtung | Typische Route |
|---|---|---|
| 214 (`0xD6`) | AgIO → AOG | `127.0.0.1:17777` → `127.0.0.1:15555` |
| 211 (`0xD3`), 212 (`0xD4`), 253 (`0xFD`), 250 (`0xFA`), 234 (`0xEA`), 221 (`0xDD`), 222 (`0xDE`), 240 (`0xF0`) | Module/TC → AOG | Modulnetz → AgIO `:9999` → Loopback `:15555` |
| 254 (`0xFE`), 252 (`0xFC`), 251 (`0xFB`), 239 (`0xEF`), 238 (`0xEE`), 236 (`0xEC`), 235 (`0xEB`), 229 (`0xE5`), 228 (`0xE4`), 241-243 (`0xF1..0xF3`) | AOG → Module/TC | AOG `:15555` → AgIO `:17777` → Broadcast `x.x.x.255:8888` |
| 100 (`0x64`) | AOG → GPS_Out | AOG Loopback zu lokalen Konsumenten (u. a. `GPS_Out`) |
| 121/123/126 (Hello), 203 (Scan Reply) | Module ↔ AgIO | Modul `:8888` ↔ AgIO `:9999` |

## Sonderfälle / Laufzeitbehandlung (wichtig für Parser)

| PGN/ID | Sonderbehandlung |
|---|---|
| 214 (`0xD6`) | viele Sentinel-Werte: `double.MaxValue`, `float.MaxValue`, `ushort.MaxValue`, `short.MaxValue`; zusätzlich `float.MinValue` setzt Roll auf 0. |
| 214 (`0xD6`) | Empfang wird über `udpWatchLimit` gedrosselt (zu schnelle Pakete werden verworfen/gezählt). |
| 211 (`0xD3`) | wird nur bei `data.Length == 14` verarbeitet; sonst verworfen. |
| 212 (`0xD4`) | nur wirksam wenn Byte 5 == 1; dann IMU auf „disconnected“-Ersatzwerte gesetzt. |
| 253 (`0xFD`) | wird nur bei `data.Length == 14` verarbeitet; Heading `9999` und Roll `8888` bedeuten „nicht verfügbar“. |
| 250 (`0xFA`) | wird nur bei `data.Length == 14` verarbeitet. |
| 221 (`0xDD`) | Anzeige erfolgt nur bei aktivierten Hardware-Messages; sonst wird Nachricht verborgen. |
| 222 (`0xDE`) | Befehle werden nur ausgeführt, wenn entsprechende Maskenbits gesetzt sind. |
| 234 (`0xEA`) | wird nur bei `data.Length == 14` verarbeitet; sonst verworfen. |
| 240 (`0xF0`) | Payload ist variabel; Heartbeat wird nur mit min. 2 Datenbytes als gültig betrachtet. |
| 100 (`0x64`) | zwei Payload-Varianten (`16`/`24`); `fix2fixHeading` nur bei `24` Byte. |
| 100 (`0x64`) | `GPS_Out` behandelt Daten als „connected“ nur, wenn letzte Nachricht < 4s alt ist. |

---

## PGN 214 (0xD6) – GPS Position (AgIO → AOG)

| Bytes | Feld | Datentyp | Bytelänge | Endian | Skalierung | Einheit | Hinweis |
|---|---|---:|---:|---|---|---|---|
| 5-12 | Longitude | float64 | 8 | LE | 1:1 | ° | `double.MaxValue` = ungültig |
| 13-20 | Latitude | float64 | 8 | LE | 1:1 | ° | `double.MaxValue` = ungültig |
| 21-24 | Heading Dual | float32 | 4 | LE | 1:1 | ° | `float.MaxValue` = ungültig |
| 25-28 | Heading True | float32 | 4 | LE | 1:1 | ° | Single-Antenna Heading |
| 29-32 | Speed | float32 | 4 | LE | 1:1 | m/s oder km/h (abhängig Senderkontext) | in AOG als Fahrgeschwindigkeit genutzt |
| 33-36 | Roll | float32 | 4 | LE | 1:1 | ° | `float.MinValue` kann Reset signalieren |
| 37-40 | Altitude | float32 | 4 | LE | 1:1 | m |  |
| 41-42 | Satellites | uint16 | 2 | LE | 1:1 | count | `ushort.MaxValue` = ungültig |
| 43 | Fix Quality | uint8 | 1 | - | 1:1 | code | `byte.MaxValue` = ungültig |
| 44-45 | HDOP | uint16 | 2 | LE | `x0.01` | - |  |
| 46-47 | Age | uint16 | 2 | LE | `x0.01` | s |  |
| 48-49 | IMU Heading | uint16 | 2 | LE | `x0.1` | ° | `ushort.MaxValue` = ungültig |
| 50-51 | IMU Roll | int16 | 2 | LE | `x0.1` | ° | `short.MaxValue` = ungültig |
| 52-53 | IMU Pitch | int16 | 2 | LE | 1:1 | raw | Implementierungsabhängig |
| 54-55 | IMU Yaw Rate | int16 | 2 | LE | 1:1 | raw | Implementierungsabhängig |

## PGN 211 (0xD3) – Externe IMU (Modul → AOG)

| Bytes | Feld | Typ | Länge | Endian | Skalierung | Einheit |
|---|---|---:|---:|---|---|---|
| 5-6 | Heading | int16 | 2 | LE | `x0.1` | ° |
| 7-8 | Roll | int16 | 2 | LE | `x0.1` | ° |
| 9-10 | Angular Velocity | int16 | 2 | LE | `/ -2` | raw/deg/s (modulabhängig) |

## PGN 212 (0xD4) – IMU Disconnect

| Bytes | Feld | Typ | Länge | Bedeutung |
|---|---|---:|---:|---|
| 5 | disconnect | uint8 | 1 | `1` = IMU abgemeldet |

## PGN 253 (0xFD) – Steer-Modul Rückmeldung

| Bytes | Feld | Typ | Länge | Endian | Skalierung | Einheit | Hinweis |
|---|---|---:|---:|---|---|---|---|
| 5-6 | Actual Steer Angle | int16 | 2 | LE | `x0.01` | ° |  |
| 7-8 | Heading | int16 | 2 | LE | `x0.1` | ° | `9999` = N/A |
| 9-10 | Roll | int16 | 2 | LE | `x0.1` | ° | `8888` = N/A |
| 11 | Switch Status | bitfield uint8 | 1 | - | - | - | bit0=Work, bit1=Steer |
| 12 | PWM Actual | uint8 | 1 | - | 1:1 | raw | |

## PGN 250 (0xFA) – Sensor Data

| Bytes | Feld | Typ | Länge |
|---|---|---:|---:|
| 5 | SensorData | uint8 | 1 |

## PGN 234 (0xEA) – Remote Switches

| Bytes | Feld | Typ | Länge | Hinweis |
|---|---|---:|---:|---|
| 5-12 | switchData | byte[8] | 8 | Abschnittsschalter / Fernschalter |

## PGN 221 (0xDD) – Display Message

| Bytes | Feld | Typ | Länge | Einheit/Bedeutung |
|---|---|---:|---:|---|
| 4 | payloadLen | uint8 | 1 | Anzahl Payload-Bytes |
| 5 | displayTime | uint8 | 1 | Sekunden |
| 6 | color | uint8 | 1 | `0=Salmon`, sonst `Bisque` |
| 7.. | message | UTF-8 | variabel | Text |

## PGN 222 (0xDE) – Remote Commands

| Bytes | Feld | Typ | Länge | Bedeutung |
|---|---|---:|---:|---|
| 5 | mask | bitfield uint8 | 1 | bit0=nudge, bit1=cycle |
| 6 | command | bitfield uint8 | 1 | bit0 links/rechts-Richtung |

## PGN 240 (0xF0) – ISOBUS Heartbeat (TC → AOG)

| Bytes | Feld | Typ | Länge | Bedeutung |
|---|---|---:|---:|---|
| 5 | status | bitfield uint8 | 1 | bit0=section enabled, bits1..3=clients |
| 6 | numberOfSections | uint8 | 1 | 0 = kein Gerät |
| 7.. | section bitmasks | byte[] | variabel | 1 Bit pro Section |

## PGN 241 (0xF1) – ISOBUS Section Control Enable (AOG → TC)

| Bytes | Feld | Typ | Länge | Bedeutung |
|---|---|---:|---:|---|
| 5 | enabled | uint8 | 1 | `0x01` an, `0x00` aus |

## PGN 242 (0xF2) – ISOBUS Process Data (AOG → TC)

| Bytes | Feld | Typ | Länge | Endian | Einheit |
|---|---|---:|---:|---|---|
| 5-6 | identifier | uint16 | 2 | LE | Kennung (z. B. 513, 397, 597) |
| 7-10 | value | int32 | 4 | LE | Prozesswert |

## PGN 243 (0xF3) – ISOBUS Field Name (AOG → TC)

| Bytes | Feld | Typ | Länge | Bedeutung |
|---|---|---:|---:|---|
| 4 | nameLen | uint8 | 1 | 0..248 |
| 5.. | name | UTF-8 | variabel | Feldname, `len=0` => Feld geschlossen |

## PGN 208 (0xD0) – Latitude/Longitude compact (definiert)

| Bytes | Feld | Typ | Länge | Endian | Skalierung |
|---|---|---:|---:|---|---|
| 5-8 | latitudeEncoded | int32 | 4 | LE | `lat = encoded / (0x7FFFFFFF/90.0)` |
| 9-12 | longitudeEncoded | int32 | 4 | LE | `lon = encoded / (0x7FFFFFFF/180.0)` |

## PGN 100 (0x64) – Corrected Position (AOG → Loop/GPS_Out)

| Bytes | Feld | Typ | Länge | Endian | Einheit | Hinweis |
|---|---|---:|---:|---|---|---|
| 5-12 | longitude | float64 | 8 | LE | ° | |
| 13-20 | latitude | float64 | 8 | LE | ° | |
| 21-28 | fix2fixHeading | float64 | 8 | LE | ° | nur wenn `length=24` |

Hinweis: Im Code wird hierfür `correctedPosition[3] = 0x64` gesetzt.

## PGN 254 (0xFE) – AutoSteer Data (AOG → Modul)

| Bytes | Feld | Typ | Länge | Endian | Skalierung |
|---|---|---:|---:|---|---|
| 5 | speedLo | uint8 | 1 | - | LSB |
| 6 | speedHi | uint8 | 1 | - | MSB |
| 7 | status | uint8 | 1 | - | Statusbits |
| 8 | steerAngleLo | uint8 | 1 | - | LSB |
| 9 | steerAngleHi | uint8 | 1 | - | MSB |
| 10 | lineDistance | uint8 | 1 | - | raw |
| 11 | sections1to8 | bitfield uint8 | 1 | - |  |
| 12 | sections9to16 | bitfield uint8 | 1 | - |  |

## PGN 252 (0xFC) – AutoSteer Settings (AOG → Modul)

| Bytes | Feld | Typ | Länge | Hinweis |
|---|---|---:|---:|---|
| 5 | gainProportional | uint8 | 1 | Kp |
| 6 | highPWM | uint8 | 1 | Max PWM |
| 7 | lowPWM | uint8 | 1 | Low PWM |
| 8 | minPWM | uint8 | 1 | Min PWM |
| 9 | countsPerDegree | uint8 | 1 | WAS counts/deg |
| 10 | wasOffsetLo | uint8 | 1 | LSB |
| 11 | wasOffsetHi | uint8 | 1 | MSB |
| 12 | ackerman | uint8 | 1 | Ackerman |

## PGN 251 (0xFB) – AutoSteer Config (AOG → Modul)

| Bytes | Feld | Typ | Länge | Skalierung/Einheit | Bedeutung/Hinweis |
|---|---|---:|---:|---|---|
| 5 | set0 | bitfield uint8 | 1 | - | Hauptkonfiguration Lenkmodul (siehe Bitmapping unten) |
| 6 | maxPulse | uint8 | 1 | raw | Grenzwert Turn-Sensor/Encoder (`setArdSteer_maxPulseCounts`) |
| 7 | minSpeed | uint8 | 1 | `x0.1 km/h` | Mindestgeschwindigkeit für AutoSteer (`setAS_minSteerSpeed`) |
| 8 | set1 | bitfield uint8 | 1 | - | Zusatzkonfiguration (Danfoss/Sensoren/Axis) |
| 9 | angVel | uint8 | 1 | bool | `1` wenn Constant-Contour aktiv, sonst `0` |

**set0 Bitbelegung (PGN 251 Byte 5):**
- bit0: Invert WAS
- bit1: Invert Steer Relays
- bit2: Invert Steer Direction
- bit3: Conv Type (`0` Differential, `1` Single)
- bit4: Motor Driver (`0` IBT2, `1` Cytron)
- bit5: Steer Enable via Switch
- bit6: Steer Enable via Button
- bit7: Encoder enabled

**set1 Bitbelegung (PGN 251 Byte 8):**
- bit0: Danfoss mode
- bit1: Pressure Sensor enabled
- bit2: Current Sensor enabled
- bit3: Axis select (`0` X, `1` Y)

## PGN 239 (0xEF) – Machine Data (AOG → Modul)

| Bytes | Feld | Typ | Länge | Skalierung/Einheit | Bedeutung/Hinweis |
|---|---|---:|---:|---|---|
| 5 | uturn | uint8 | 1 | bool | `0` kein U-Turn aktiv, `1` U-Turn aktiv |
| 6 | speed | uint8 | 1 | `x0.1 km/h` | Fahrgeschwindigkeit für Maschinenlogik |
| 7 | hydLift | uint8 | 1 | enum | `0` neutral/aus, `1` senken, `2` heben |
| 8 | tram | bitfield uint8 | 1 | - | Tramline-Steuerbits (`controlByte`) |
| 9 | geoStop | uint8 | 1 | bool | `1` wenn Out-of-Bounds (GeoFence Stop) |
| 11 | sc1to8 | bitfield uint8 | 1 | - | Sektionen 1..8 an/aus |
| 12 | sc9to16 | bitfield uint8 | 1 | - | Sektionen 9..16 an/aus |

**Tram-Bits (PGN 239 Byte 8):**
- bit0: rechte Tramline aktiv
- bit1: linke Tramline aktiv

## PGN 238 (0xEE) – Machine Config (AOG → Modul)

| Bytes | Feld | Typ | Länge | Skalierung/Einheit | Bedeutung/Hinweis |
|---|---|---:|---:|---|---|
| 5 | raiseTime | uint8 | 1 | s (modulseitig) | Dauer für Hydraulik-Heben |
| 6 | lowerTime | uint8 | 1 | s (modulseitig) | Dauer für Hydraulik-Senken |
| 7 | enableHyd | uint8 | 1 | bool (legacy) | historisches Feld; Hyd-Enable wird praktisch über `set0` bit1 geführt |
| 8 | set0 | bitfield uint8 | 1 | - | Maschinen-Grundkonfiguration (siehe Bitmapping) |
| 9 | user1 | uint8 | 1 | raw | frei nutzbarer User-Parameter 1 |
| 10 | user2 | uint8 | 1 | raw | frei nutzbarer User-Parameter 2 |
| 11 | user3 | uint8 | 1 | raw | frei nutzbarer User-Parameter 3 |
| 12 | user4 | uint8 | 1 | raw | frei nutzbarer User-Parameter 4 |

**set0 Bitbelegung (PGN 238 Byte 8):**
- bit0: Invert Machine Relays
- bit1: Hydraulic Lift enabled

## PGN 236 (0xEC) – Relay Config (AOG → Modul)

| Bytes | Feld | Typ | Länge | Hinweis |
|---|---|---:|---:|---|
| 5..28 | pin0..pin23 | uint8[24] | 24 | Pinbelegung |

## PGN 235 (0xEB) – Section Dimensions (AOG → Modul)

| Bytes | Feld | Typ | Länge | Endian | Skalierung |
|---|---|---:|---:|---|---|
| 5..36 | sec0..sec15 (Lo/Hi) | uint16[16] | 32 | LE | üblicherweise Breite * 100 |
| 37 | numSections | uint8 | 1 | - | Anzahl Sektionen |

## PGN 229 (0xE5) – Extended Section Control (AOG → Modul)

| Bytes | Feld | Typ | Länge |
|---|---|---:|---:|
| 5 | sc1to8 | bitfield uint8 | 1 |
| 6 | sc9to16 | bitfield uint8 | 1 |
| 7 | sc17to24 | bitfield uint8 | 1 |
| 8 | sc25to32 | bitfield uint8 | 1 |
| 9 | sc33to40 | bitfield uint8 | 1 |
| 10 | sc41to48 | bitfield uint8 | 1 |
| 11 | sc49to56 | bitfield uint8 | 1 |
| 12 | sc57to64 | bitfield uint8 | 1 |
| 13 | toolLSpeed | uint8 | 1 |
| 14 | toolRSpeed | uint8 | 1 |

## PGN 228 (0xE4) – Rate Control (AOG → Modul)

| Bytes | Feld | Typ | Länge |
|---|---|---:|---:|
| 5 | rate0 | uint8 | 1 |
| 6 | rate1 | uint8 | 1 |
| 7 | rate2 | uint8 | 1 |

---

## Netzwerk-/Service-IDs in AgIO (kein klassischer Fahrdaten-PGN, aber im gleichen Byte-3-Kanal)

### ID 126 – Hello AutoSteer Modul (UDP → AgIO, Länge 11)
| Bytes | Feld | Typ | Bedeutung |
|---|---|---:|---|
| 2 | source | uint8 | Modul-ID (bei Hello-AutoSteer typ. 126) |
| 5-6 | actualSteerAngle | int16 LE | Aktueller Lenkwinkel (`x0.01°`) für Ping-Ansicht |
| 7-8 | wasCounts | int16 LE | Lenkwinkelsensor-Zähler |
| 9 | switchStatus | bitfield uint8 | bit0=WorkSwitch, bit1=SteerSwitch |

### ID 123 – Hello Machine Modul (UDP → AgIO, Länge 11)
| Bytes | Feld | Typ | Bedeutung |
|---|---|---:|---|
| 2 | source | uint8 | Modul-ID (Machine) |
| 5 | sections1to8 | bitfield uint8 | Anzeige der Section-Zustände 1..8 |
| 6 | sections9to16 | bitfield uint8 | Anzeige der Section-Zustände 9..16 |

### ID 121 – Hello IMU Modul (UDP → AgIO, Länge 11)
| Bytes | Feld | Typ | Bedeutung |
|---|---|---:|---|
| 2 | source | uint8 | Modul-ID (IMU) |
| (keine weiteren Nutzdaten ausgewertet) | - | - | Dient primär als „alive/ping“-Signal |

### ID 203 – Scan Reply (UDP → AgIO, Länge 13)
| Bytes | Feld | Typ | Bedeutung |
|---|---|---:|---|
| 2 | moduleType | uint8 | 126=Steer, 123=Machine, 121=IMU, 120=GPS |
| 5-8 | moduleIP | uint8[4] | Gemeldete Modul-IP |
| 9-11 | subnet3 | uint8[3] | Erste 3 Bytes der Subnetzmaske; Anzeige als `x.y.z` |

## Zusammengesetzte IDs im GPS_Out-Empfangspfad

`GPS_Out` bildet zuerst ein 16-bit „Outer PGN“ aus Byte0/Byte1 und danach ein 16-bit `SubPGN` aus Byte2/Byte3:

| Name | Dezimal | Hex | Bedeutung |
|---|---:|---:|---|
| Outer PGN | 33152 | `0x8180` | AOG-Frame erkannt |
| SubPGN | 54908 | `0xD67C` | AgIO NMEA-Übersetzung (entspricht D6 mit Source 0x7C) |
| SubPGN | 25727 | `0x647F` | Corrected Position (PGN `0x64` mit Source `0x7F`) |

---

## Speziell: PGN 64 vs 0x64

- **PGN 64 (dezimal)** wurde im untersuchten Codepfad nicht als eigenes Nachrichtenformat gefunden.
- Gefunden und jetzt explizit gelistet wurde **PGN 100 (`0x64`)** im `GPS/GPS_Out`-Pfad.
