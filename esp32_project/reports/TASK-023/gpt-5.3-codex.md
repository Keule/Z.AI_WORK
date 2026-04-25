Entwickler-Report für Task TASK-023

Entwickler: GPT-5.3-Codex
Task-ID: TASK-023

Checkliste (Pflichtfelder)

- [x] Zusammenfassung ausgefüllt
- [x] Geänderte Dateien vollständig aufgelistet
- [x] Tests / Build dokumentiert (inkl. Ergebnis)
- [x] Offene Fragen / Probleme dokumentiert (oder explizit „keine“)

Zusammenfassung

- Boot-Initialisierung im ESP32-HAL auf bedarfsgeführtes Capability-Gating umgestellt:
  - Gemeinsamer Bootpfad initialisiert Sensor-SPI nicht mehr pauschal.
  - Sensor-SPI wird nur dann initialisiert, wenn aktive Module IMU/WAS/Aktorik benötigen.
  - IMU-, Steering-Angle- und Actuator-Init werden in `hal_esp32_init_all()` je Capability einzeln ausgeführt oder mit Log übersprungen.
- GNSS-Buildup-Pfad bleibt schlank (Comm + GNSS-UART) und profitiert davon, dass der Common-Bootpfad keine Sensor-SPI-Init mehr erzwingt.
- Prompt-Compliance-Stichprobe durchgeführt: Der effektive Task-Prompt (Button-Ausgabe im aktuellen Auftrag) enthält weiterhin den Pflicht-Onboarding-Block Teil 1 mit den drei Pflichtdokumenten (`README.md`, `docs/process/PLAN_AGENT.md`, `docs/process/QUICKSTART_WORKFLOW.md`).

Geänderte Dateien
- `src/hal_esp32/hal_impl.cpp`
- `reports/TASK-023/gpt-5.3-codex.md`

Tests / Build
- `python3 tools/check_task_context.py --task-id TASK-023` (OK)
- `pio run` (nicht ausführbar: `pio` in der Umgebung nicht installiert)
- `SKIP_PROFILE_BUILDS=1 python3 tools/run_test_matrix.py` (OK, Host-Smoke-Matrix)
- Prompt-Compliance-Stichprobe (manuell): Effektiver Prompttext enthält Onboarding Teil 1 inkl. der drei Pflichtquellen (OK).

Offene Fragen / Probleme
- Keine.
