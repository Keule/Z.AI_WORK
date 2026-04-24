---
Task ID: 1
Agent: main (Z.ai Code)
Task: ESP32 AgSteer Projekt kompilieren und Fehler beheben

Work Log:
- ZIP-File entpackt nach /home/z/my-project/esp32_project/
- PlatformIO installiert (pip install platformio)
- ESP32 Plattform und Toolchain heruntergeladen (espressif32@6.13.0, toolchain-xtensa-esp32@8.4.0)
- Erster Kompilierungsversuch: 20+ Fehler in config_*.cpp, cmd_system.cpp, imu.cpp, diagnostics.cpp, main.cpp
- Systematische Fehleranalyse und Behebung in mehreren Iterationen

Stage Summary:
- Projekt kompiliert erfolgreich mit `pio run -e profile_full_steer_ntrip`
- Firmware-Ausgabe: .pio/build/profile_full_steer_ntrip/firmware.elf + .bin

Behobene Fehler (11 Dateien, 15+ Fehler):

1. **log_config.h** — `LOG_LEVEL_CFG` fehlte (9 Dateien referenzierten es)
   → `#define LOG_LEVEL_CFG LOG_LVL_DEBUG` hinzugefügt

2. **config_actuator.cpp** — `std::strcasecmp` nicht in std::, `Stream` unbekannt
   → `#include <strings.h>`, `#include <Arduino.h>`, `strcasecmp` ohne `std::`

3. **config_framework.cpp** — Gleiche Probleme wie config_actuator.cpp
   → Gleiche Fixes

4. **config_network.cpp, config_pid.cpp, config_system.cpp, config_gnss.cpp, config_ntrip.cpp** — Alle `Stream*` ohne `#include <Arduino.h>`
   → `#include <Arduino.h>` hinzugefügt

5. **cmd_system.cpp** — `std::strcmp` nicht in std::, `OpMode::ACTIVE` → `OP_MODE_ACTIVE`, `setupWizardRequestStart` undeclared
   → `#include <cstring>`, `#include "setup_wizard.h"`, `strcmp` statt `std::strcmp`, `OP_MODE_ACTIVE`/`OP_MODE_PAUSED`

6. **imu.cpp** — Anonymer Namespace nicht geschlossen
   → `imu_ops` aus Namespace herausgezogen, `namespace {}` korrekt geschlossen

7. **diagnostics.cpp** — `FEAT_ENABLED` in `#if` ohne `features.h`, `extern "C"` im Funktionskörper, `sdLoggerGetOverflowCount` Linkage-Mismatch
   → `#include "features.h"`, `extern "C"` Deklaration auf File-Scope verschoben

8. **main.cpp** — Config-Header ohne `logic/` Präfix, `controlTaskFunc`/`commTaskFunc` nicht deklariert
   → Include-Pfade korrigiert, Forward-Deklarationen hinzugefügt

9. **hal_bno085.cpp** — Lokale C++ Forward-Deklaration statt `extern "C"` via `hal_esp32_internal.h`
   → Lokale Deklarationen entfernt, `#include "hal_esp32_internal.h"` hinzugefügt

10. **hal_spi.cpp** — `hal_esp32_sensor_spi_port()` Definition ohne explizites `extern "C"`
    → `extern "C"` zur Definition hinzugefügt

Verbleibende Warnungen (nicht kritisch):
- `LOG_LOCAL_LEVEL` redefined in mehreren hal_esp32/*.cpp (Info-Warnungen, kein Fehler)
