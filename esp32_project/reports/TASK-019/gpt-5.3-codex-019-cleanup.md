# Entwickler-Report – TASK-019 (gnss_buildup Bereinigung)

**Entwickler:** GPT-5.3-Codex  
**Datum:** 2026-04-17  
**Task-ID:** TASK-019

## 1) Vorher/Nachher Flags (`[env:gnss_buildup]`)

### Vorher
- `-DFEAT_PROFILE_COMM_ONLY`
- `-DFEAT_COMM`
- `-DFEAT_GNSS_UART_MIRROR`
- `-DGNSS_MIRROR_BAUD=115200`
- `-DGNSS_MIRROR_UART1_RX_PIN=44`
- `-DGNSS_MIRROR_UART1_TX_PIN=-1`
- `-DGNSS_MIRROR_UART2_RX_PIN=2`
- `-DGNSS_MIRROR_UART2_TX_PIN=-1`
- `-DFEAT_GNSS`
- `-DFEAT_GNSS_BUILDUP`

### Nachher
- `-DFEAT_PROFILE_COMM_ONLY`
- `-DFEAT_GNSS_UART_MIRROR`
- `-DFEAT_GNSS_BUILDUP`

## 2) Betroffene Dateien
- `platformio.ini`
- `include/hardware_pins.h`
- `src/main.cpp`
- `docs/process/GNSS_BUILDUP.md`

## 3) Begründung je entfernter Redundanz

1. **`-DFEAT_COMM` entfernt**  
   Im Profil ist bereits `FEAT_PROFILE_COMM_ONLY` aktiv. Über die zentrale Feature-Normalisierung in `src/logic/features.h` wird damit Kommunikation implizit aktiviert, auch ohne separates `-DFEAT_COMM`.

2. **`-DFEAT_GNSS` entfernt**  
   Für den GNSS-Buildup-Pfad ist die explizite Modusaktivierung über `FEAT_GNSS_BUILDUP` ausreichend; es existiert keine zusätzliche, zwingende Codepfad-Abhängigkeit im Buildup-Flow auf ein separates Build-Define `FEAT_GNSS`.

3. **Pin-/Baud-Defines aus Buildflags entfernt**  
   `GNSS_MIRROR_BAUD` und alle `GNSS_MIRROR_UART*_..._PIN` Werte wurden aus `platformio.ini` entfernt und als zentrale Konstanten nach `include/hardware_pins.h` verlagert. Damit gilt ein einziger Source-of-Truth für Hardwarebelegung.

## 4) Build-/Smoke-Ergebnis

- `pio run -e gnss_buildup`  
  Ergebnis: **nicht ausführbar** in dieser Container-Umgebung (`pio: command not found`).

- `python3 -m platformio run -e gnss_buildup`  
  Ergebnis: **nicht ausführbar** in dieser Container-Umgebung (`No module named platformio`).

- `git diff --check`  
  Ergebnis: **ok** (keine Whitespace-/Patchformat-Probleme).

## 5) Ergebnisbewertung

- Profil `gnss_buildup` bleibt aktivierbar auf Konfigurationsebene.
- GNSS-Mirror nutzt jetzt zentrale Pin-/Baudkonstanten im Code.
- Keine weiteren Environments wurden in ihren Feature-Sets verändert.
