Entwickler-Report für Task TASK-024

Entwickler: GPT-5.3-Codex
Task-ID: TASK-024

Checkliste (Pflichtfelder)

- [x] Zusammenfassung ausgefüllt
- [x] Geänderte Dateien vollständig aufgelistet
- [x] Tests / Build dokumentiert (inkl. Ergebnis)
- [x] Offene Fragen / Probleme dokumentiert (oder explizit „keine“)

Zusammenfassung

- In `src/hal_esp32/hal_impl.cpp` wurde ein deterministisches Pin-Claim-System ergänzt.
- Pro Init-Pfad (`imu_bringup`, `gnss_buildup`, `full_init`) werden Pin-Claims vor der eigentlichen Initialisierung geprüft.
- Bei Konflikten werden klare Fehlerlogs erzeugt und der betroffene Init-Pfad abgebrochen.
- Für den GNSS-Buildup-Pfad ist ein definierter Fallback enthalten: Wenn benutzerdefinierte RX/TX-Pins kollidieren, wird auf die UART-Defaultpins zurückgefallen; wenn auch diese kollidieren, wird die UART-Initialisierung abgebrochen.
- Die GNSS-Buildup-Defaultpins wurden auf die zentralen Makros `GNSS_UART1_RX`/`GNSS_UART1_TX` umgestellt (statt Magic Numbers).
- In `include/hardware_pins.h` wurde eine kurze, normative Init-Pfad-Pin-Claim-Matrix ergänzt.

Geänderte Dateien

- `src/hal_esp32/hal_impl.cpp`
- `include/hardware_pins.h`
- `reports/TASK-024/gpt-5.3-codex.md`

Tests / Build

- `python3 tools/check_task_context.py --task-id TASK-024` → OK.
- `git diff --check` → OK.
- `pio run` → fehlgeschlagen, da `pio` in der Umgebung nicht installiert/verfügbar ist (`/bin/bash: pio: command not found`).
- Onboarding-Regression-Check (String-/Pfadprüfung) via `rg` ausgeführt; Pflicht-Onboarding-Referenzen in den relevanten Task-Prompt-Taskdateien (`TASK-022`, `TASK-023`) weiterhin vorhanden.

Offene Fragen / Probleme

- In dieser Laufumgebung ist kein PlatformIO (`pio`) verfügbar; vollständiger Build-Check muss in einer Umgebung mit installiertem PlatformIO nachgezogen werden.
- Die tatsächliche Prompt-Generator-Quelldatei für „klickbare Codex-Buttons“ ist im aktuellen Repository nicht eindeutig auffindbar; überprüfbar waren die referenzierenden Task-/Prompt-Spezifikationsdateien.
