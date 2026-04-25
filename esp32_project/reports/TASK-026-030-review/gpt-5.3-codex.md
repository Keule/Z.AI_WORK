Reviewer-Report: TASK-026 bis TASK-030 Integration

Reviewer: GPT-5.3-Codex
Datum: 2026-04-21
Branch: work

## Scope
Review der integrierten Umsetzung von TASK-026, TASK-027, TASK-028, TASK-029, TASK-030 gegen Task-ACs und ADR-001/002/003.

Geprüfte Quellen (Auszug):
- backlog/tasks/TASK-026-fw-config-und-board-profile-restrukturierung.md
- backlog/tasks/TASK-027-modul-system-mit-runtime-aktivierung-und-pin-claim-arbitrierung.md
- backlog/tasks/TASK-028-soft-config-mit-nutzer-defaults-und-runtime-konfiguration.md
- backlog/tasks/TASK-029-maintask-fuer-blocking-ops-und-psram-sd-logging.md
- backlog/tasks/TASK-030-task025-backlog-status-und-ntrip-architektur-anpassung.md
- docs/adr/ADR-001-config-layering-fw-soft-runtime.md
- docs/adr/ADR-002-task-model-control-comm-maint.md
- docs/adr/ADR-003-feature-module-system-and-pin-claims.md
- src/main.cpp, src/logic/modules.cpp, src/logic/hw_status.cpp, src/logic/ntrip.cpp, include/soft_config.h, src/hal_esp32/hal_impl.cpp

## Ergebnis
Status: **Nicht freigabefähig** (3 kritische Findings).

---

## Findings

### F1 (Kritisch): Feature-Module können nach `hal_esp32_init_all()` faktisch nicht aktivieren

**Beobachtung**
- `setup()` ruft `hal_esp32_init_all()` auf und danach `moduleActivate(MOD_...)` für mehrere Module.
- HAL claimt Pins im Full-Init bereits mit Legacy-Ownern (z. B. `"imu-int"`, `"eth-cs"`).
- `moduleActivate()` erlaubt nur idempotente Re-Claims mit **identischem** Owner (`MOD_IMU`, `MOD_ETH`, ...), ansonsten harter Konflikt + Abbruch.

**Folge**
- Standard-Aktivierungen in `setup()` laufen in Konflikte und schlagen fehl.
- Das widerspricht dem beabsichtigten Runtime-Modulsystem aus TASK-027/030 (inkl. Dependency-Kette für NTRIP).

**Evidenz (Dateistellen)**
- `setup()` aktiviert Module nach Full-Init in fester Reihenfolge.
- HAL Full-Init setzt Legacy-Claims.
- Modulsystem fordert bei bereits geclaimten Pins Owner-Gleichheit und bricht bei abweichendem Owner ab.

**Empfehlung**
- Entweder Owner-Tags in HAL auf `MOD_*` harmonisieren, **oder** eine dokumentierte Übergangsregel per ADR einführen (Legacy-Owner-Whitelist während Migration).

---

### F2 (Kritisch): GNSS/NTRIP-Status wird als Fehler gesetzt, auch wenn Verbindung OK ist

**Beobachtung**
- In `ntripTick()` ruft der CONNECTED-Zweig `hwStatusSetFlag(HW_GNSS, HW_SEV_OK)` auf.
- `hwStatusSetFlag()` setzt intern immer `error=true` (unabhängig von Severity).
- `hwStatusUpdate()` löscht `HW_GNSS` nur, wenn NTRIP-Modul inaktiv ist, nicht bei erfolgreicher Verbindung.

**Folge**
- GNSS-Subsystem kann im Fehlerzähler verbleiben bzw. fälschlich als Fehler markiert werden.
- AC aus TASK-030 („NTRIP-Status connected/disconnected/error wird berücksichtigt“) ist semantisch nicht korrekt umgesetzt.

**Empfehlung**
- CONNECTED-Zweig auf `hwStatusClearFlag(HW_GNSS)` ändern.
- Error-Pfade weiterhin via `hwStatusSetFlag(HW_GNSS, HW_SEV_WARNING/ERROR)`.

---

### F3 (Kritisch): `soft_config.h` enthält produktive Zugangsdaten statt neutraler Defaults

**Beobachtung**
- `include/soft_config.h` enthält nicht-leere Defaultwerte für NTRIP Host/Mountpoint/User/Password.
- TASK-028 fordert explizit leere Defaults für NTRIP-Zugangsdaten.
- Damit werden Secrets/produktive Werte in Firmware-Defaults kodiert.

**Folge**
- Verstoß gegen Task-AC (TASK-028).
- Sicherheits- und Betriebsrisiko (Credentials im Repo/Artefakt).

**Empfehlung**
- Defaults auf leer setzen (`""`, Port optional neutral).
- Secrets nur über Override-Pfade (SD/Serial/WebUI/NVS) laden.

---

## Positive Punkte
- Trennung `fw_config`/`soft_config`/`RuntimeConfig` ist strukturell vorhanden.
- `maintTask` ist eingeführt, NTRIP-Tick in Maint-Pfad verlagert.
- `TASK-025` Status im Backlog ist konsolidiert (`done`).

## Abschluss
Ohne Behebung von F1–F3 sollte die Integration 26–30 nicht als abgeschlossen/freigegeben gelten.
