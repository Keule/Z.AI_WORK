Entwickler-Report für Task TASK-019E

Entwickler: GPT-5.3-Codex
Task-ID: TASK-019E

## Zusammenfassung

Für TASK-019E wurde ein verbindlicher Smoke-Test-Reportstandard für den GNSS-Buildup als ausfüllbarer Report mit Pflichtfeldern dokumentiert (Setup, Firmwarestand, Evidenz, Befunde, Verdict, Blocker/Follow-up). Die Test- und Reporting-Anforderungen aus `backlog/tasks/TASK-019E-smoke-test-reportstandard.md` wurden als explizite Prüfpunkte in diesen Report übernommen.

## Reportstandard (verbindlich)

### 1) Testkontext / Setup
- Testdatum (UTC): `2026-04-17`
- Board / Hardware-Revision: `ESP32-S3-Board (genaue Rev: OFFEN)`
- Buildziel (PlatformIO-Environment): `gnss_buildup`
- Firmwarestand:
  - Git-Branch: `work`
  - Commit: `3f1f5e2`
- Pinmap-Referenz: `TASK-019A` (reale Verdrahtung muss gegen die dortige Pinbelegung abgeglichen werden)

### 2) Reale Pinbelegung / Wiring (Pflicht)
| Signal | Board-Pin | UM980-Pin | Status |
|---|---:|---:|---|
| UART1_TX -> UM980_RX | OFFEN | OFFEN | BLOCKER |
| UART1_RX <- UM980_TX | OFFEN | OFFEN | BLOCKER |
| UART2_TX -> UM980_RX | OFFEN | OFFEN | BLOCKER |
| UART2_RX <- UM980_TX | OFFEN | OFFEN | BLOCKER |
| GND | OFFEN | OFFEN | BLOCKER |
| Versorgung | OFFEN | OFFEN | BLOCKER |

Hinweis: Reale Verdrahtungsdaten liegen in dieser Session nicht vor und müssen vor einem Hardware-Smoke-Test nachgetragen werden.

### 3) UART-/Console-Parameter (Pflicht)
- USB-Console Baudrate: `115200` (aus Codepfad)
- GNSS-UART Baudrate: `115200` (Sollwert für Bringup, sofern nicht abweichend projektspezifisch gesetzt)
- Framing: `8N1`
- Mirror-Zustand beim Startlog: `PFLICHTANGABE (aktiv/inaktiv + Flag)`

### 4) Build- und Testdurchführung (TASK-019E Verifikation)
- [x] Review gegen `templates/dev-report.md` erfolgt.
- [x] Review gegen `docs/process/QUICKSTART_WORKFLOW.md` erfolgt.
- [x] Trockenlauf mit Beispielreport ohne inhaltliche Lücken durchgeführt.
- [ ] Hardware-Smoke-Test ausgeführt.

### 5) Evidenz: Bootlog (Pflicht)
Status: **nicht erbracht (BLOCKER)**, da kein Hardwarezugang in der Session.

Soll-Format (Beispielauszug):
```text
[BOOT] build_env=gnss_buildup
[BOOT] mirror_uart1=true mirror_uart2=true
[HAL]  GNSS RTCM UART1 ready (baud=115200, mode=8N1, rx=<pin>, tx=<pin>)
[HAL]  GNSS RTCM UART2 ready (baud=115200, mode=8N1, rx=<pin>, tx=<pin>)
```

### 6) Evidenz: Nachweis beider UART-Streams (Pflicht)
Status: **nicht erbracht (BLOCKER)**, da keine Laufzeitlogs von realer Hardware vorliegen.

Soll-Nachweis:
- UART1-Stream als Console-Mirror sichtbar (mit Timestamp/Präfix).
- UART2-Stream als Console-Mirror sichtbar (mit Timestamp/Präfix).
- Beide Streams innerhalb derselben Session und mit nachvollziehbarer Zuordnung dokumentiert.

### 7) Stabilitätsdauer & Auffälligkeiten (Pflicht)
- Laufdauer Ziel: mindestens `15 min` unter aktivem Mirror.
- Tatsächlich erreicht: `0 min` (nicht durchgeführt).
- Auffälligkeiten:
  - BLOCKER: Kein Hardwarezugang, daher keine Aussagen zu Überläufen, Latenzspitzen oder Log-Blockierung möglich.

### 8) Verdict / Freigabestatus
- **Verdict:** `BLOCKED`
- **Begründung:** Pflicht-Evidenzen (Bootlog + Nachweis UART1/UART2 + Stabilitätslauf) konnten ohne Hardwarezugang nicht erzeugt werden.

## Geänderte Dateien
- reports/TASK-019/gpt-5.3-codex.md

## Tests / Build
- Dokumentations-/Strukturprüfung des Reports gegen:
  - `backlog/tasks/TASK-019E-smoke-test-reportstandard.md`
  - `templates/dev-report.md`
  - `docs/process/QUICKSTART_WORKFLOW.md`
- Kein Firmware-Build/Flash in dieser Session, da Fokus auf Reportstandard und fehlender Hardwarezugang.

## Offene Fragen / Probleme (explizit)
1. **Blocker:** Exakte reale Pinbelegung (Board <-> UM980) für UART1/UART2 fehlt.
2. **Blocker:** Reale Bootlogs mit aktivem Mirror fehlen.
3. **Blocker:** Realer Nachweis beider UART-Streams in einer Session fehlt.
4. **Blocker:** Stabilitätsmessung (>=15 min) unter Last fehlt.
5. **Klärung nötig:** Soll-Baudrate UART2 bei Mirror identisch zu UART1 (115200) oder projektspezifisch abweichend?

## Follow-up (für Mensch/Reviewer)
- Hardware verbinden gemäß TASK-019A-Pinmap.
- Build mit `pio run -e gnss_buildup`.
- Flash + Monitor starten, Bootlog erfassen.
- UART1- und UART2-Mirror in einer Session nachweisen (Loganhang).
- 15-Minuten-Stabilitätslauf dokumentieren (inkl. Auffälligkeiten/Fehlerzähler).
# Entwickler-Report – TASK-019C

**Entwickler:** GPT-5.3-Codex  
**Datum:** 2026-04-17  
**Task-ID:** TASK-019C

## Kurzfassung
Es wurde ein klar aktivierbarer GNSS-Buildup-Modus mit reduziertem Initialisierungspfad umgesetzt. Der Modus initialisiert ausschließlich Kommunikation (ETH/UDP) und GNSS-RTCM-UART, lässt Sensor-/Aktorik-Init aus, liefert diagnostische Start-/Status-Logs und definiert ein reproduzierbares Fallback bei Init-Timeout.

## Gegenüberstellung: Normalmodus vs. GNSS-Buildup-Modus

| Aspekt | Normalmodus | GNSS-Buildup-Modus |
|---|---|---|
| Aktivierung | Standard-Build ohne `FEAT_GNSS_BUILDUP` | Compile-Time-Flag `FEAT_GNSS_BUILDUP` (z. B. via `env:gnss_buildup`) |
| HAL-Init in `setup()` | `hal_esp32_init_all()` | `hal_esp32_init_gnss_buildup()` |
| Subsysteme | Sensor-SPI, IMU, Lenkwinkel, Aktuator, Netzwerk | Nur Netzwerk + GNSS-RTCM-UART |
| Task-Start | `control` + `comm` | Nur `comm` |
| OTA/Kalibrierung/Moduldetektion | Aktiv (normaler Pfad) | Übersprungen (reduzierter Bringup-Pfad) |
| Diagnoselogs | reguläre Laufzeitlogs | explizite GNSS-Buildup-Logs: Init-Status, Port-Status, GNSS-Fix-Status |
| Fallback | normaler Betriebsfluss | Bei Timeout degradierter Diagnosebetrieb ohne Abort/Reboot |

## Technische Details
- Neuer HAL-Entry-Point: `hal_esp32_init_gnss_buildup()` mit ETH-Init und GNSS-RTCM-UART-Init.
- GNSS-Buildup-Defaults (über `-D...` übersteuerbar): UART1, 115200 Baud, RX=45, TX=48.
- `main.cpp` enthält jetzt einen klaren Modusschalter für GNSS-Buildup sowie eine gegenseitige Ausschlussprüfung mit IMU-Bringup.
- Im GNSS-Buildup-Modus werden nur benötigte Pfade ausgeführt; dadurch keine Sensor-/Aktorik-Initialisierung.
- Fallback-Verhalten: nach `MAIN_GNSS_BUILDUP_INIT_TIMEOUT_MS` (15s) ohne `net && rtcm_uart` wird degradierter Bringup-Betrieb geloggt und weitergeführt.

## Verifikation
- Buildaufruf für `gnss_buildup` versucht (`pio run -e gnss_buildup` / `python3 -m platformio run -e gnss_buildup`), jedoch in der Umgebung ohne installiertes PlatformIO-CLI/-Modul nicht ausführbar.
- Regressionstest für Standard-Environment aus gleichem Grund hier nicht lokal ausführbar; Änderungen sind auf klar getrennte Moduspfade begrenzt.
Offene Fragen / Probleme
- Keine inhaltlichen Blocker für den Backlog-Stand.
- Rollenhinweis: `backlog/README.md` weist Task-Neuanlage dem KI-Planer zu; Umsetzung erfolgte hier auf explizite Arbeitsanweisung.

---

Task-ID: TASK-019B
Titel: PlatformIO-Environment `gnss_buildup` für GNSS-Buildup
Datum: 2026-04-17

Umsetzung
- Neues PlatformIO-Environment `gnss_buildup` auf Basis `env:T-ETH-Lite-ESP32S3` in `platformio.ini` ergänzt.
- Für das Profil wurden ausschließlich Comm+GNSS-Flags gesetzt:
  - `-DFEAT_PROFILE_COMM_ONLY`
  - `-DFEAT_COMM`
  - `-DFEAT_GNSS`
- Damit bleiben Steering/IMU/Aktorik in diesem Profil deaktiviert (keine entsprechenden Flags gesetzt).
- Bestehende Profile blieben unverändert; es wurde nur ein zusätzlicher Profil-Eintrag ergänzt.

Build-Befehle und Ergebnis
- `pio run -e gnss_buildup`
  - Ergebnis: fehlgeschlagen, Tool nicht verfügbar (`pio: command not found`).
- `python3 -m platformio run -e gnss_buildup`
  - Ergebnis: fehlgeschlagen, Python-Modul nicht installiert (`No module named platformio`).

Blocker
- Lokale Umgebung enthält keine PlatformIO-CLI/kein PlatformIO-Python-Modul, daher kein vollständiger Build-Nachweis im Container möglich.
