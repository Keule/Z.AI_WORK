/**
 * @file sd_ota.h
 * @brief SD-Card OTA Firmware Update – public API.
 *
 * Provides functions to check for a firmware update file on an SD card
 * and perform a blockwise OTA update into the inactive flash partition.
 *
 * Usage (from main.cpp / setup):
 *   if (isFirmwareUpdateAvailableOnSD()) {
 *       updateFirmwareFromSD();   // blocks until reboot
 *   }
 *
 * SD card is accessed over SPI2_HOST (SD_SPI_BUS), which is temporarily
 * borrowed from the sensor bus during the update.
 *
 * No Arduino / ESP32 headers in this file – see sd_ota_esp32.cpp for
 * the ESP32 implementation.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ===================================================================
// Version helpers
// ===================================================================

/// Struct for a semantic version (major.minor.patch).
typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} SdOtaVersion;

/// Parse a version string like "1.2.3" into a SdOtaVersion struct.
/// Returns true on success, false if the string is malformed.
/// If parsing fails, out is zero-initialised.
bool sdOtaParseVersion(const char* str, SdOtaVersion* out);

/// Compare two versions.
/// Returns > 0 if a > b, < 0 if a < b, 0 if equal.
int sdOtaCompareVersion(const SdOtaVersion* a, const SdOtaVersion* b);

/// Get the compile-time firmware version (defined via -DFIRMWARE_VERSION).
/// Falls back to "0.0.0" if not defined.
SdOtaVersion sdOtaGetCurrentVersion(void);

// ===================================================================
// Public API
// ===================================================================

/// Check whether a firmware update file exists on the SD card.
///
/// Looks for /firmware.bin first, then /update.bin.
/// If /firmware.version exists, additionally checks that the SD
/// firmware version is strictly greater than the current (compile-time)
/// firmware version.  If the version check fails, returns false
/// (update not needed).
///
/// This function does NOT modify any state – it is safe to call
/// from any context.
///
/// @return true if a valid update file is available on SD.
bool isFirmwareUpdateAvailableOnSD(void);

/// Perform a firmware update from the SD card.
///
/// 1. Releases the sensor SPI bus (SPI2_HOST / SENS_SPI_BUS).
/// 2. Initialises the SD card over SPI2_HOST with SD-card pins.
/// 3. Reads the firmware file blockwise and writes it to the
///    inactive OTA partition via the ESP32 Update API.
/// 4. On success: validates the image, sets the boot flag, and
///    reboots (this function never returns on success).
/// 5. On failure: restores the sensor SPI bus and returns false.
///
/// WARNING: This function blocks for several seconds and must NOT be
/// called while the control task is running.  Call it from setup()
/// BEFORE creating FreeRTOS tasks.
///
/// @return false on any error (update not applied, old firmware kept).
bool updateFirmwareFromSD(void);

#ifdef __cplusplus
}
#endif
