Entwickler-Report für Task TASK-022

Entwickler: GPT-5.3-Codex
Task-ID: TASK-022

Checkliste (Pflichtfelder)

- [x] Zusammenfassung ausgefüllt
- [x] Geänderte Dateien vollständig aufgelistet
- [x] Tests / Build dokumentiert (inkl. Ergebnis)
- [x] Offene Fragen / Probleme dokumentiert (oder explizit „keine“)

Zusammenfassung

- Compile-Time-Gating für zusätzliche SPI/UART-Capabilities in `src/logic/features.h` ergänzt:
  - `FEAT_CAP_SENSOR_SPI2` wird aus Modulbedarf (Sensor/Aktor) oder explizitem Define abgeleitet.
  - `FEAT_CAP_GNSS_UART_MIRROR` wird nur aktiv, wenn `FEAT_GNSS_BUILDUP` + Mirror-Flag gesetzt sind.
- Nutzung der neuen Capability-Makros integriert:
  - `src/main.cpp`: GNSS-Mirror und SPI-Telemetrie-Logging werden compile-time gegated.
  - `src/hal_esp32/hal_impl.cpp`: Sensor-SPI-Init erfolgt nur bei aktiver SPI2-Capability.
- `platformio.ini` ergänzt:
  - Capability-Mapping dokumentiert.
  - GNSS-Buildup-Profile setzen `-DFEAT_GNSS`, damit UART-Mirror-Capability an expliziten Modulbedarf gekoppelt ist.
- Promptquelle für klickbare Codex-Task-Prompts angelegt:
  - `tools/codex_task_prompt_buttons.html` mit Button-basiertem Prompt-Generator inkl. Pflicht-Onboarding Teil 1.

Geänderte Dateien

- `src/logic/features.h`
- `src/main.cpp`
- `src/hal_esp32/hal_impl.cpp`
- `platformio.ini`
- `tools/codex_task_prompt_buttons.html`
- `reports/TASK-022/gpt-5.3-codex.md`

Tests / Build

- `python3 tools/check_task_context.py --task-id TASK-022` → OK.
- `rg -n "README.md|docs/process/PLAN_AGENT.md|docs/process/QUICKSTART_WORKFLOW.md|Pflicht-Onboarding Teil 1" tools/codex_task_prompt_buttons.html -S` → Pflicht-Onboarding-Referenzen vorhanden.
- `pio run` → nicht ausführbar in dieser Umgebung (`pio: command not found`).
- `pio run -e gnss_buildup_s3` → nicht ausführbar in dieser Umgebung (`pio: command not found`).

Offene Fragen / Probleme

- Umgebung enthält kein PlatformIO-CLI (`pio`). Build-/Profil-Verifikation muss in CI oder lokaler Entwicklerumgebung mit installiertem PlatformIO nachgezogen werden.
