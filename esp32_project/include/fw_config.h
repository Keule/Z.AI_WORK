/**
 * @file fw_config.h
 * @brief Firmware configuration entry point.
 *
 * Central include for board-specific pin definitions and firmware-wide settings.
 * Board profile is selected via PlatformIO build flags (-DLILYGO_T_ETH_LITE_ESP32S3
 * or -DLILYGO_T_ETH_LITE_ESP32), see board_profile_select.h.
 */

#pragma once

#include <cstdint>
#include "board_profile/board_profile_select.h"

// ---------------------------------------------------------------------------
// Firmware OTA files on SD card
// ---------------------------------------------------------------------------
#define SD_FW_FILE_PRIMARY   "/firmware.bin"
#define SD_FW_FILE_ALT       "/update.bin"

// Version file on SD card (optional - contains e.g. "1.2.3")
#define SD_FW_VERSION_FILE   "/firmware.version"
