# Board-Auswahl & TFT/ETH-Profil-Strategie

## Kurzantwort auf die Architekturfrage

Ja: Die `#define`-Werte aus `Setup216_LilyGo_ETH_Lite_ESP32.h` können wir grundsätzlich übernehmen.

Aber: Statt die originale Setup-Datei ungeprüft in die Bibliothek zu kopieren, ist für dieses Repo die robustere Variante:

1. **Projekt-eigene Board-Konfigdatei** anlegen (z. B. `include/board_profile/tft_eth_profile_lilygo_t_eth_lite_esp32*.h`).
2. Nur die wirklich benötigten Defines aus Setup216 übernehmen (Pinning, Treiber, Frequenzen).
3. Aktivierung über **Build-Flags pro Environment** in `platformio.ini`.

So bleiben Library-Updates von TFT_eSPI einfacher und wir vermeiden, dass lokale Änderungen in `lib/TFT_eSPI` bei Updates überschrieben werden.

## Warum nicht direkt in `hardware_pins.h`?

`hardware_pins.h` ist aktuell die zentrale Quelle für board-nahe Basispins (ETH/Sensorik/IMU).

Für TFT/ETH-bezogene Makros ist eine **separate Board-Profil-Datei** sauberer, weil:
- TFT_eSPI eigene Makro-Namen und Setup-Logik hat,
- wir Board-Profile klar trennen und Setup-Defines nicht in der Basis-Pin-Datei vermischen.

Empfehlung:
- `hardware_pins.h` für allgemeine HW-Pins beibehalten,
- zusätzlich `board_profile/*.h` für TFT/ETH-Profil-Defines verwenden.

## Wie beim Kompilieren Board-wählbar machen?

Im Repo ist das schon angelegt: Die Environments setzen Board-Makros.

- `T-ETH-Lite-ESP32` setzt `-DLILYGO_T_ETH_LITE_ESP32`.
- `T-ETH-Lite-ESP32S3` setzt `-DLILYGO_T_ETH_LITE_ESP32S3`.

Build-Auswahl:

```bash
pio run -e T-ETH-Lite-ESP32
pio run -e T-ETH-Lite-ESP32S3
```

Für Upload entsprechend:

```bash
pio run -e T-ETH-Lite-ESP32S3 -t upload
```

Im Code erfolgt die Auswahl zentral über `include/board_profile/board_profile_select.h` per `#if defined(LILYGO_T_ETH_LITE_...)` und bindet dann das passende `tft_eth_profile_*` Headerfile ein.
