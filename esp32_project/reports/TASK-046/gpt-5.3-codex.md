Entwickler-Report für Task TASK-046

Entwickler: GPT-5.3-Codex (KI-Planer/KI-Reviewer)
Task-ID: TASK-046

Checkliste (Pflichtfelder)

- [x] Zusammenfassung ausgefüllt
- [x] Geänderte Dateien vollständig aufgelistet
- [x] Tests / Build dokumentiert (inkl. Ergebnis)
- [x] Offene Fragen / Probleme dokumentiert (oder explizit „keine“)

## Zusammenfassung

Dieser Report konserviert nachträglich die Prozess- und Qualitätsbewertung der gesamten Refactor-Welle seit Start der KI-Arbeit.

Durchgeführt wurden:
1. Anlage eines dedizierten Nacharbeits-Tasks mit Marker `[#process-hygiene]` (TASK-046).
2. Vollreview der bisherigen Refactor-Welle (Code + beobachtete Laufzeitlogs aus Nutzerfeedback).
3. Strukturierte Findings-Matrix inkl. Priorisierung und Nacharbeitsvorschlägen.

Ziel war, die bislang chatgetriebenen Erkenntnisse dauerhaft repo-fähig zu dokumentieren, damit Folgeagenten/Menschen ohne Chatkontext weiterarbeiten können.

---

## Scope des Reviews

### Betrachteter Änderungsbereich
- Groß-Commit der Refactor-Welle (`a06a6c0`):
  - Modularchitektur (`ModuleOps`)
  - Feature-Gates
  - Control-Step-Unbundling
  - `NavigationState`-Substrukturen
  - CLI/NVS/Setup/Diag
  - Boot-Maintenance (Web/BT)
  - Test-/Doku-Erweiterungen

### Zusätzlich berücksichtigte Evidenz
- Laufzeit-/Bootlogs aus Nutzer-Tests nach OTA und Konfigmodus.
- Build-/Runtime-Hinweise, die auf regressionsartige Effekte hindeuten.

---

## Geänderte Dateien (für TASK-046)

- `backlog/tasks/TASK-046-process-hygiene-post-hoc-review-und-backlog-konservierung-refactor-welle.md`
- `backlog/index.yaml`
- `reports/TASK-046/gpt-5.3-codex.md`

---

## Findings-Matrix (Review gesamt)

### F-01 (High) — Fehlende Prozesskonservierung für die Refactor-Welle
**Befund**: Umfangreiche Änderungen wurden initial ohne durchgängige taskbasierte Nachverfolgbarkeit in einem dedizierten Post-hoc-Prozess-Task zusammengeführt.

**Risiko**: Erhöhte Onboarding-Kosten und Fehlinterpretationen für Folgeagenten ohne Chat-Historie.

**Empfehlung**: Mit TASK-046 geschlossen; bei weiteren Refactor-Blöcken frühzeitig Prozesshygiene-Task anlegen.

---

### F-02 (High) — NVS-Persistenzfehler durch Key-Längenrisiko
**Befund**: In Feldlogs trat `ESP_ERR_NVS_KEY_TOO_LONG` für `ntrip_mountpoint` auf.

**Auswirkung**: `save`-Pfad schlägt fehl; Runtime-Konfig nicht persistent.

**Empfehlung**:
- kurze NVS-Keys (<= 15 Zeichen) verwenden,
- Legacy-Fallback beim Laden,
- Commit-/Set-Fehler pro Key loggen.

**Status**: als kritischer Nacharbeitsbedarf in Review markiert; in Folgearbeit bereits adressiert, aber weiterhin hardwareseitig zu verifizieren.

---

### F-03 (Medium) — BT-CLI I/O-Asymmetrie
**Befund**: BT konnte Befehle senden, aber Antworten kamen initial nur auf Serial.

**Auswirkung**: Eingeschränkte Nutzbarkeit im Feld (Telefonterminal ohne Feedback).

**Empfehlung**: CLI-Output-Stream abstrahieren und je Inputkanal routen; für Subsysteme mit direktem `Serial` (z. B. Spezial-Console) Mirror-Konzept anwenden.

---

### F-04 (Medium) — UART-Konsole im Bootmodus zeitweise ohne sichtbare Ausgabe
**Befund**: UM980-Liveconsole konnte unter Inputlast „verhungern“ oder nur auf einem Kanal sichtbar sein.

**Auswirkung**: Schlechtere Diagnosefähigkeit im Konfigmodus.

**Empfehlung**: zyklisches Tick ohne Input-Gating + optionales Mirroring auf BT beibehalten; später ggf. vereinheitlichte Console-Abstraktion (Single Source of Output).

---

### F-05 (Medium) — Boot-Maintenance wächst schneller als formale Spezifikation
**Befund**: WiFi/Bluetooth/Web-OTA/ETH-URL/Boot-CLI sind funktional stark gewachsen, aber Spezifikation/AC-Matrix über mehrere Aufgaben verteilt.

**Auswirkung**: Hohe Kopplung zwischen `main.cpp`, CLI, UART-Setup und NVS-Flow; Regressionen schwer punktuell zu testen.

**Empfehlung**: separaten technischen Konsolidierungs-Task für Boot-Maintenance-Subsystem mit klaren ACs, Interfacegrenzen und Testkatalog.

---

### F-06 (Low) — Feature-Gate/Hardware-Erkennung und Fehlersignale gemischt
**Befund**: Logs zeigen teilweise „not compiled“ und trotzdem Hardware-nahe Fehlerpfade (z. B. ungültige Pins, Recovery-Sequenzen), was die Lesbarkeit erschwert.

**Auswirkung**: Diagnoseaufwand erhöht.

**Empfehlung**: Logging vereinheitlichen: „not compiled“ strikt vor HW-Zugriffen, klare Trennung von „compiled-off“, „hw missing“, „init fail“.

---

## Gesamtbewertung der Refactor-Welle

### Positiv
- Saubere strategische Richtung: modulare Architektur, Feature-Gating, bessere Testbarkeit.
- Deutlich verbesserte Diagnose- und Bringup-Fähigkeiten (CLI, Setup, Boot-Maintenance).
- Gute Grundlage für weitere Stabilisierung durch zielgerichtete Nacharbeit.

### Kritisch/verbesserungswürdig
- Hohe Änderungsdichte in einem Schritt hat mehrere Integrationskanten geöffnet (NVS, BT/Serial-Routing, UART-Liveconsole, Bootpfad).
- Prozesskonservierung wurde initial nicht auf dem Niveau der technischen Eingriffstiefe gehalten.

---

## Priorisierte Nacharbeit (abgeleitet)

1. **P1**: Verbindliche Hardware-Revalidierung der Boot-Maintenance-Usecases (ESP32 classic + S3) inkl. Save/Load, UART-Liveconsole, BT-CLI I/O, Web-OTA über WiFi+ETH.
2. **P1**: Mini-Regressionstestplan für `main.cpp`-Bootpfad (Konfigmodus EIN/AUS, Safety LOW/HIGH, SD vorhanden/nicht vorhanden).
3. **P2**: Konsolidierung der Ausgabewege (Serial/BT/UART-console) über gemeinsames Output-Routing-Modell.
4. **P2**: Dokumentation der endgültigen NVS-Key-Konvention inkl. Längenregeln + Migrationsstrategie.

---

## Tests / Build

Durchgeführt im aktuellen Container:
- Struktur-/Konsistenzprüfung neuer Artefakte (`TASK-046` in Taskdatei + Index + Report).
- `pio`-Buildausführung war im Container nicht möglich (`pio: command not found`).

Empfohlene Nachprüfung auf Buildfähiger Umgebung:
- `pio run -e profile_full_steer_ntrip`
- `pio run -e T-ETH-Lite-ESP32`
- Feldtest: Boot-Maintenance-Pfad mit BT + Web-OTA + Save/Load.

---

## Offene Fragen / Probleme

1. Ist die Boot-Maintenance-Funktionalität (WiFi/ETH/BT/UART/NVS) final als ein zusammenhängendes Subsystem akzeptiert, oder soll sie in dedizierte Teil-Tasks zerlegt werden?
2. Soll für BT-Unterstützung eine harte Zielplattformmatrix dokumentiert werden (Classic/S3/C3/C6/H2), um erwartetes Verhalten pro Target explizit zu machen?
3. Soll die Review-Policy bei ähnlich großen Refactor-Blöcken verpflichtend vorsehen, dass parallel ein `[#process-hygiene]`-Task mitgeführt wird?
