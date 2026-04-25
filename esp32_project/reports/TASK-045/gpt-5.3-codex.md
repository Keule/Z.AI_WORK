# TASK-045 — Entwickler-Report (gpt-5.3-codex)

## Umsetzung

Zur Entschärfung des Watchdog-Risikos auf ESP32 Classic wurden zwei Kernpunkte umgesetzt:

1. **maintTask-Start entkoppelt von SD-Modul**
   - `sdLoggerMaintInit()` startet den kombinierten Maint-Task nun auch ohne `MOD_SD`.
   - Der Maint-Loop führt ETH/NTRIP weiterhin aus; SD-Logging wird nur bei aktivem `MOD_SD` verarbeitet.
   - Dadurch bleibt `ntripTick()` im vorgesehenen Hintergrundkontext verfügbar.

2. **WDT-Konfiguration explizit gesetzt**
   - `sdkconfig.defaults` ergänzt um:
     - `CONFIG_ESP_TASK_WDT_EN=y`
     - `CONFIG_ESP_TASK_WDT_TIMEOUT_S=15`
     - `CONFIG_ESP_TASK_WDT_PANIC=y`

Zusätzlich in `main.cpp`:
- Maint-Task-Startbedingung auf `(MOD_SD || MOD_NTRIP)` geändert.
- Bei fehlender Wartungs-Relevanz wird dies explizit geloggt.

## Geänderte Dateien

- `src/hal_esp32/sd_logger_esp32.cpp`
- `src/main.cpp`
- `sdkconfig.defaults`

## Erwarteter Effekt

- NTRIP-Connect/Reconnect verbleibt in `maintTask` statt in zeitkritischen Pfaden.
- Weniger Risiko, dass Core-0-IDLE durch blockierende/lastige Wartungsarbeit verhungert.
- WDT-Timeout ist explizit dokumentiert und reproduzierbar.
