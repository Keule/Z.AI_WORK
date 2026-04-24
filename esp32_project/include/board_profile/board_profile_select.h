#pragma once

// Boardprofil-Auswahl fuer TFT/ETH-bezogene Defines.
// Aktivierung erfolgt ueber PlatformIO-Buildflags, z.B.:
//   -DLILYGO_T_ETH_LITE_ESP32S3
//   -DLILYGO_T_ETH_LITE_ESP32

#if defined(LILYGO_T_ETH_LITE_ESP32S3)
  #include "LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h"
#elif defined(LILYGO_T_ETH_LITE_ESP32)
  #include "LILYGO_T_ETH_LITE_ESP32_board_pins.h"
#else
  #error "Kein unterstuetztes Boardprofil gesetzt. Definiere LILYGO_T_ETH_LITE_ESP32S3 oder LILYGO_T_ETH_LITE_ESP32."
#endif
