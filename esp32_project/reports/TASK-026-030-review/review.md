# Kombinierter Review — TASK-026..030 Integration

**Datum:** 2026-04-21  
**Reviewer-Quellen:**
1. `reports/TASK-026-030-review/gpt-5.3-codex.md`
2. KI-Review Super Z / Z.ai (Textvergleich aus Chat, Branch-Stand `81d4427`)

---

## 1) Ziel des kombinierten Reviews

Dieser kombinierte Review konsolidiert beide Review-Ergebnisse zu:
- **Gemeinsamen Befunden** (hohe Übereinstimmung),
- **Abweichungen in Bewertung/Schweregrad**,
- **Klarer Nacharbeitsliste** für Planer/Mensch/Entwickler.

---

## 2) Gemeinsame Befunde (Konsens)

### K1 — Legacy-HAL-Claims vs Feature-Module-Claims sind ungelöst
Beide Reviews bestätigen einen Übergangskonflikt:
- HAL-Init claimt Pins mit Legacy-Ownern (`imu-cs`, `eth-sck`, ...).
- Feature-Module claimen dieselben Pins mit `MOD_*` Ownern.
- Bei harter Konfliktpolitik (ADR-HAL-001) kann `moduleActivate()` dadurch nach `hal_esp32_init_all()` fehlschlagen.

**Konsens:** Es braucht eine explizite Übergangsstrategie (ADR/Task), nicht nur Kommentare.

### K2 — Produktive NTRIP-Zugangsdaten in `soft_config.h`
Beide Reviews identifizieren Klartext-Zugangsdaten in Defaults (Host/Mount/User/Pass).

**Konsens:** Sicherheits-/Betriebsrisiko; Defaults sollten neutral sein und echte Zugangsdaten über Override-Pfade kommen.

### K3 — GPIO-46 Konflikt (IMU_INT vs LOG_SWITCH)
Beide Reviews erkennen den Konflikt als technisch detektiert, aber nicht produktseitig entschieden.

**Konsens:** Mensch/Planer müssen entscheiden, ob
- Board-Restriktion bewusst akzeptiert und formal dokumentiert wird,
- oder Pinbelegung geändert wird.

### K4 — MaintTask/PSRAM-Logging Architektur ist grundsätzlich umgesetzt
Beide Reviews sehen TASK-029 Kernarchitektur als implementiert:
- MaintTask existiert,
- NTRIP-Tick im Maint-Pfad,
- PSRAM-Buffer + SD-Flush-Pfad vorhanden.

---

## 3) Abweichungen zwischen den Reviews

### D1 — Schweregrad NTRIP/HW-Status-Semantik
- **Codex-Review:** kritisch (CONNECTED setzt `hwStatusSetFlag(HW_GNSS, HW_SEV_OK)`; Set-API markiert intern dennoch Error-Flag).
- **Z.ai-Review:** eher „teilweise/ok mit offenen Punkten“.

**Kombinierte Bewertung:** **hoch priorisierte Nacharbeit**, da Status-Semantik eindeutig und testbar definiert werden muss.

### D2 — Merge-Reife
- **Codex-Review:** „nicht freigabefähig“ bis F1–F3 geklärt.
- **Z.ai-Review:** „keine harten Blocker, aber offene Punkte vor development-merge klären“.

**Kombinierte Bewertung:** Für `development` sollte mindestens die Nacharbeit zu Claim-Übergang + Credentials + Status-Semantik geplant und terminiert sein (DoD-Gate).

### D3 — NTRIP→GNSS Dependency als Muss
- **Z.ai-Review:** schlägt zusätzliche GNSS-Dependency für NTRIP vor.
- **Codex-Review:** sieht das eher als Verbesserung, nicht als expliziten ADR-Verstoß.

**Kombinierte Bewertung:** als **Design-Entscheidung offen** markieren; falls eingeführt, über ADR-GNSS präzisieren.

---

## 4) Konsolidiertes Urteil

**Technischer Stand:** solide Basis, aber mit 3 prioritären Integrationsrisiken:
1. Claim-Migrationskonflikt Legacy ↔ `MOD_*`
2. NTRIP/HW-Status-Semantik im CONNECTED-Pfad
3. Produktive NTRIP-Credentials als Defaults

**Empfehlung:** Kein „silent close“ von TASK-026..030 ohne geplante Nacharbeit mit klaren ACs.

---

## 5) Konkrete Nacharbeit (ohne sofortige Umsetzung)

### P1 — Planer/Architektur
1. **Übergangs-ADR** erstellen: Legacy-Owner → `MOD_*` Strategie
   - Optionen: Owner-Harmonisierung, Alias/Whitelist mit Sunset, Init-Reihenfolge ändern.
2. **TASK-027/030 ACs schärfen**
   - explizite Aktivierungserwartung nach Full-Init,
   - explizite Zustandssemantik für GNSS/NTRIP-HW-Status.

### P2 — Security/Config
1. **TASK-028 Nachtrag**: NTRIP Defaults neutral/leer als AC-Gate.
2. Secrets nur über `softConfigLoadOverrides()`/Runtime-Quelle.

### P3 — Doku/Backlog
1. GPIO-46 Konflikt formal dokumentieren (bewusste Restriktion oder Rewire-Task).
2. Profilbenennung in `platformio.ini` bereinigen (irreführende Namen korrigieren).

### P4 — Verifikation
1. Build- und Smoke-Test-Matrix für relevante Environments fest als Gate.
2. Negativtests für Claim-Konflikte + Statusübergänge ergänzen.

---

## 6) Entscheidungsbedarf Mensch/Planer

1. Wird Legacy-Claim-Kompabilität als Übergang erlaubt? Wenn ja: wie lange und wie dokumentiert?
2. Sind produktive NTRIP-Daten im Repo grundsätzlich verboten (Policy)?
3. GPIO-46: Board-Restriktion akzeptieren oder Hardware-/Pin-Task priorisieren?
4. Soll NTRIP neben ETH auch explizit GNSS als Modul-Dependency erhalten?

---

## 7) Abschluss

Beide Reviews sind in der Kernaussage kompatibel: **Architektur ist tragfähig, aber Übergangs-/Policy-Punkte sind noch nicht sauber abgeschlossen.**  
Dieser kombinierte Review dient als gemeinsame Grundlage für den nächsten Planungszyklus.
