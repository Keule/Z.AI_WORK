# TASK-019 Entwickler-Report (ESP32-WROVER-E ETH Build)

## 1) Verwendete Primärquellen (Arduino/LilyGO)

1. LilyGO T-ETH-Series (offizielles Repository, README/Build-Hinweise):
   - https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series
   - Relevanz: Board-Matrix (ESP32-WROVER-E vs S3), Empfehlung Arduino-ESP32 >= 3.0.0, ETHClass2-Fallback-Hinweis.
2. LilyGO Wiki T-ETH-Lite (ESP32):
   - https://wiki.lilygo.cc/get_started/en/High_speed/T-ETH-Lite/T-ETH-Lite/T-ETH-Lite.html
   - Relevanz: Arduino-Boardannahme „ESP32 Dev Module“ für ESP32-Variante.
3. arduino-esp32 offizielles W5500-Beispiel:
   - https://github.com/espressif/arduino-esp32/blob/master/libraries/Ethernet/examples/ETH_W5500_Arduino_SPI/ETH_W5500_Arduino_SPI.ino
   - Relevanz: `ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI...)` Initialisierungspfad.

## 2) `platformio.ini` – vorher / nachher

### Vorher (relevante Punkte)
- `T-ETH-Lite-ESP32` nutzte `board = esp32dev`.
- ESP32-Env erzwang TFT_eSPI-Setup via:
  - `-DUSER_SETUP_LOADED`
  - `-include lib/TFT_eSPI/User_Setups/Setup216_LilyGo_ETH_Lite_ESP32.h`
- `gnss_buildup` zeigte auf S3 (`extends = env:T-ETH-Lite-ESP32S3`).

### Nachher (relevante Punkte)
- `T-ETH-Lite-ESP32` auf WROVER-E-Basis umgestellt:
  - `board = esp-wrover-kit`
- Ethernet-Defines für ESP32-Build explizit gesetzt:
  - `-DETH_PHY_TYPE=ETH_PHY_W5500`
  - `-DETH_PHY_ADDR=1`
- TFT-Zwangsinclude entfernt (kein harter Header-Zwang für GNSS/ETH-Build).
- `gnss_buildup` auf ESP32-Basis umgestellt (`extends = env:T-ETH-Lite-ESP32`).
- Zur Sicherung des bisherigen S3-Ablaufs zusätzlich `gnss_buildup_s3` ergänzt.

## 3) Code-/Include-Anpassungen

- FreeRTOS-Header auf ESP-IDF-konforme Form vereinheitlicht:
  - `<FreeRTOS.h>` -> `<freertos/FreeRTOS.h>`
- Betroffene Dateien:
  - `src/main.cpp`
  - `src/hal_esp32/sd_logger_esp32.cpp`

## 4) Build-Ergebnis (ESP32 + S3)

### Geplant
- `pio run -e T-ETH-Lite-ESP32`
- `pio run -e gnss_buildup`
- `pio run -e T-ETH-Lite-ESP32S3`
- `pio run -e gnss_buildup_s3`

### Tatsächlich im aktuellen Container
- Build-Tooling nicht vorhanden (`pio: command not found`, `python -m platformio: No module named platformio`).
- Daher kein vollständiger Compile-Nachweis im Container möglich.

## 5) Offene Restpunkte

1. CI/Build-Host mit PlatformIO bereitstellen und Matrix-Build fahren (ESP32 + S3).
2. Optional: echte Board-spezifische Pinheader trennen (`hardware_pins.h` für ESP32/S3), um Laufzeit-Fehlbelegung explizit zu vermeiden.
3. Falls das Zielboard tatsächlich RTL8201 statt W5500 nutzt, ETH-Pfad und Defines auf PHY-Typ des konkreten Hardware-Spin verifizieren.
