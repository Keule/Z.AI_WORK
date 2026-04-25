# TASK-044 — Entwickler-Report (gpt-5.3-codex)

## Umsetzung

Folgende Runtime-Gating-Anpassungen wurden umgesetzt:

1. `controlStep()` gate't IMU/ADS/ACT/SAFETY jetzt über `moduleIsActive(...)`.
2. Bei inaktiven Modulen werden deterministische Fallbackwerte gesetzt:
   - IMU/ADS inaktiv → kein Sensor-Read, Qualität wird nicht als „Fehler“ behandelt.
   - ACT inaktiv → kein `actuatorWriteCommand(...)`.
3. Neue Hilfsfunktion `moduleControlPipelineReady(...)` eingeführt:
   - Voraussetzung für Control-Pipeline: `MOD_IMU && MOD_ADS && MOD_ACT`.
4. `setup()` startet `controlInit()`/`controlTask` nur, wenn Pipeline wirklich aktiv ist.
5. `hwStatusUpdate(...)` erweitert:
   - Zusätzliche `*_monitored` Parameter, um „nicht aktiv“ von „Fehler“ zu trennen.
   - `main.cpp` übergibt dafür aktuelle Modulaktivitäten.

## Geänderte Dateien

- `src/logic/control.cpp`
- `src/logic/modules.h`
- `src/logic/modules.cpp`
- `src/logic/hw_status.h`
- `src/logic/hw_status.cpp`
- `src/main.cpp`

## Erwarteter Effekt

- Keine Phantom-Nutzung von IMU/ADS/ACT ohne aktive Module/Pins.
- Klareres Boot-Verhalten: Control-Pfad startet nur bei aktiver Minimalpipeline.
- Runtime-Statusmeldungen unterscheiden besser zwischen „nicht aktiv“ und „defekt“.
