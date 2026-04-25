/**
 * @file sd_ota_esp32.cpp
 * @brief ESP32-S3 implementation of SD-Card OTA firmware update.
 *
 * This file implements isFirmwareUpdateAvailableOnSD() and
 * updateFirmwareFromSD() from logic/sd_ota.h.
 *
 * SPI bus strategy:
 *   The SD card uses SPI2_HOST (SD_SPI_BUS) with its own pins (SCK=7, MISO=5, MOSI=6, CS=42).
 *   The sensor bus also uses SD_SPI_BUS but with DIFFERENT pins (SCK=47, MISO=21, MOSI=38).
 *   During the update the sensor SPI is released via hal_sensor_spi_deinit(),
 *   then SD_SPI_BUS is re-initialised with SD pins. After the update (or on any error)
 *   the sensor SPI is restored via hal_sensor_spi_reinit().
 *
 * This file includes Arduino / ESP32 headers and is only compiled
 * for the ESP32 target (NOT for PC simulation).
 */

#include "hal/hal.h"
#include "fw_config.h"
#include "logic/module_interface.h"
#include "logic/sd_ota.h"
#include "debug/DebugConsole.h"

#include "logic/log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_OTA
#include "esp_log.h"
#include "logic/log_ext.h"

// ESP32 / Arduino includes
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>

// ===================================================================
// Constants
// ===================================================================

/// Block size for reading from SD and writing to OTA flash.
/// 4 KB matches the flash erase page size and gives a good balance
/// between RAM usage and transfer speed.
static const size_t OTA_BLOCK_SIZE = 4096;

/// Maximum firmware file size we accept (sanity check).
/// Must not exceed the OTA partition size (~3.75 MB per slot).
/// Set to 3 MB as a safe limit (the actual firmware should be
/// significantly smaller).
static const size_t MAX_FW_SIZE = 3UL * 1024UL * 1024UL;

// ===================================================================
// Internal helpers
// ===================================================================

/**
 * Read the version string from /firmware.version on the SD card.
 *
 * @param file  An open SD file handle (already positioned at start).
 * @param buf   Buffer to receive the version string (null-terminated).
 * @param max_len  Buffer size.
 * @return true if a version string was read successfully.
 */
static bool readVersionFromFile(fs::File& file, char* buf, size_t max_len) {
    if (!file || max_len < 4) return false;

    size_t n = file.readBytes(buf, max_len - 1);
    if (n == 0) return false;

    buf[n] = '\0';

    // Strip trailing whitespace / newline
    while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n' || buf[n - 1] == ' ')) {
        buf[--n] = '\0';
    }

    return n > 0;
}

/**
 * Verify that the current partition table supports OTA.
 *
 * Checks that the running partition is an OTA slot (ota_0 or ota_1).
 * If the firmware was flashed with a non-OTA partition table (single
 * factory slot), OTA updates are not possible.
 *
 * @return true if OTA is supported.
 */
static bool verifyOtaSupported(void) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) {
        LOGE("OTA", "cannot determine running partition");
        return false;
    }

    if (running->type != ESP_PARTITION_TYPE_APP) {
        LOGE("OTA", "running partition is not an APP partition (type=%u)",
                running->type);
        return false;
    }

    // ESP_PARTITION_SUBTYPE_APP_OTA_0 = 0x10, OTA_1 = 0x11, etc.
    // Factory = 0x00, Test = 0x20
    if (running->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
        running->subtype > ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
        LOGW("OTA", "not booted from an OTA slot (subtype=0x%02X). "
                "Partition table may not support OTA. Continuing anyway...",
                running->subtype);
        // Don't hard-fail here – the Update library can handle it
        // if otadata partition exists.
    }

    LOGI("OTA", "running from partition '%s' (0x%06X, subtype=0x%02X)",
            running->label, running->address, running->subtype);
    return true;
}

/**
 * Check if the SD card has a firmware file that is newer than the
 * currently running firmware.
 *
 * Side effects: temporarily claims and releases SPI2_HOST.
 *
 * @return true  if a valid, newer firmware file exists on SD.
 * @return false if no file, same version, or SD error.
 */
bool isFirmwareUpdateAvailableOnSD(void) {
    if (!moduleSysIsActive(ModuleId::LOGGING)) {
        LOGW("OTA", "MOD_SD inactive -> skip SD OTA availability check");
        return false;
    }

    LOGI("OTA", "checking for firmware update on SD card...");

    // 1. Verify OTA partition support
    if (!verifyOtaSupported()) {
        return false;
    }

    // 2. Release sensor SPI bus → SPI2_HOST becomes available for SD
    hal_sensor_spi_deinit();
    hal_delay_ms(10);  // brief settle time

    // 3. Create a dedicated SPI instance for the SD card on SD_SPI_BUS (= SPI2_HOST)
    SPIClass sdSPI(SD_SPI_BUS);
    sdSPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS);

    // 4. Mount SD card
    if (!SD.begin(SD_CS, sdSPI, 4000000, "/sd", 5)) {
        LOGE("OTA", "SD card init FAILED – no card inserted or bad contact?");
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }
    LOGI("OTA", "SD card mounted OK");

    bool result = false;
    const char* found_file = nullptr;

    // 5. Check for firmware files (primary first, then alternative)
    if (SD.exists(SD_FW_FILE_PRIMARY)) {
        found_file = SD_FW_FILE_PRIMARY;
    } else if (SD.exists(SD_FW_FILE_ALT)) {
        found_file = SD_FW_FILE_ALT;
    }

    if (!found_file) {
        LOGI("OTA", "no firmware file found (checked %s and %s)",
                SD_FW_FILE_PRIMARY, SD_FW_FILE_ALT);
        goto cleanup;
    }

    // 6. Check file size
    {
        fs::File fwFile = SD.open(found_file, "r");
        if (!fwFile) {
            LOGE("OTA", "cannot open %s", found_file);
            goto cleanup;
        }

        size_t file_size = fwFile.size();
        LOGI("OTA", "found %s (%u bytes)", found_file, (unsigned)file_size);
        fwFile.close();

        if (file_size == 0 || file_size > MAX_FW_SIZE) {
            LOGW("OTA", "file size invalid (0 or > %u bytes), skipping",
                    (unsigned)MAX_FW_SIZE);
            goto cleanup;
        }
    }

    // 7. Version check (optional – if /firmware.version exists on SD)
    if (SD.exists(SD_FW_VERSION_FILE)) {
        fs::File verFile = SD.open(SD_FW_VERSION_FILE, "r");
        if (verFile) {
            char ver_str[64] = {0};
            if (readVersionFromFile(verFile, ver_str, sizeof(ver_str))) {
                SdOtaVersion sd_ver = {0, 0, 0};
                if (sdOtaParseVersion(ver_str, &sd_ver)) {
                    SdOtaVersion cur_ver = sdOtaGetCurrentVersion();
                    int cmp = sdOtaCompareVersion(&sd_ver, &cur_ver);

                    LOGI("OTA", "SD version = %u.%u.%u, current version = %u.%u.%u  (%s)",
                            sd_ver.major, sd_ver.minor, sd_ver.patch,
                            cur_ver.major, cur_ver.minor, cur_ver.patch,
                            cmp > 0 ? "NEWER" : cmp == 0 ? "SAME" : "OLDER");

                    if (cmp <= 0) {
                        LOGI("OTA", "firmware on SD is not newer, skipping update");
                        verFile.close();
                        goto cleanup;
                    }
                } else {
                    LOGW("OTA", "cannot parse version '%s', skipping version check",
                            ver_str);
                    // If version file is malformed, proceed without version check
                    // (user might have placed a file without version info)
                }
            }
            verFile.close();
        }
    } else {
        LOGI("OTA", "no %s file, skipping version check", SD_FW_VERSION_FILE);
    }

    // 8. All checks passed
    result = true;
    LOGI("OTA", "firmware update available on SD card");

cleanup:
    // 9. Unmount SD card and release SPI
    SD.end();
    sdSPI.end();

    // 10. Restore sensor SPI bus
    hal_sensor_spi_reinit();

    return result;
}

/**
 * Perform a firmware update from the SD card.
 *
 * This function blocks for several seconds (depending on firmware size
 * and SD card speed).  On success it never returns – the ESP32 reboots
 * into the new firmware.  On failure it returns false and the old
 * firmware continues to run.
 *
 * IMPORTANT: Call this BEFORE creating FreeRTOS tasks, ideally as the
 * first thing in setup() after hal_esp32_init_all().
 *
 * @return false on any error (old firmware kept).
 */
bool updateFirmwareFromSD(void) {
    if (!moduleSysIsActive(ModuleId::LOGGING)) {
        LOGW("OTA", "MOD_SD inactive -> skip SD OTA update");
        return false;
    }

    LOGI("OTA", "===== STARTING FIRMWARE UPDATE FROM SD =====");

    // -----------------------------------------------------------------
    // Phase 1: Prepare – release sensor SPI, init SD card
    // -----------------------------------------------------------------
    LOGI("OTA", "phase 1 – releasing sensor SPI bus...");
    hal_sensor_spi_deinit();
    hal_delay_ms(10);

    SPIClass sdSPI(SD_SPI_BUS);
    sdSPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS);

    if (!SD.begin(SD_CS, sdSPI, 4000000, "/sd", 5)) {
        LOGE("OTA", "FATAL – SD card init FAILED");
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }
    LOGI("OTA", "SD card mounted OK");

    // -----------------------------------------------------------------
    // Phase 2: Open firmware file
    // -----------------------------------------------------------------
    LOGI("OTA", "phase 2 – opening firmware file...");

    const char* fw_path = nullptr;
    if (SD.exists(SD_FW_FILE_PRIMARY)) {
        fw_path = SD_FW_FILE_PRIMARY;
    } else if (SD.exists(SD_FW_FILE_ALT)) {
        fw_path = SD_FW_FILE_ALT;
    } else {
        LOGE("OTA", "FATAL – no firmware file found (%s / %s)",
                SD_FW_FILE_PRIMARY, SD_FW_FILE_ALT);
        SD.end();
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }

    fs::File fwFile = SD.open(fw_path, "r");
    if (!fwFile) {
        LOGE("OTA", "FATAL – cannot open %s", fw_path);
        SD.end();
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }

    size_t file_size = fwFile.size();
    LOGI("OTA", "%s opened, size = %u bytes", fw_path, (unsigned)file_size);

    if (file_size == 0) {
        LOGE("OTA", "FATAL – firmware file is EMPTY");
        fwFile.close();
        SD.end();
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }

    if (file_size > MAX_FW_SIZE) {
        LOGE("OTA", "FATAL – firmware file too large (%u > %u bytes)",
                (unsigned)file_size, (unsigned)MAX_FW_SIZE);
        fwFile.close();
        SD.end();
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }

    // -----------------------------------------------------------------
    // Phase 3: Begin OTA write
    // -----------------------------------------------------------------
    LOGI("OTA", "phase 3 – starting OTA write to flash...");

    if (!Update.begin(file_size)) {
        LOGE("OTA", "FATAL – Update.begin(%u) failed: error=%d '%s'",
                (unsigned)file_size, Update.getError(),
                Update.errorString());
        fwFile.close();
        Update.abort();
        SD.end();
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }

    LOGI("OTA", "OTA partition initialised (%u bytes)", (unsigned)file_size);

    // -----------------------------------------------------------------
    // Phase 4: Blockwise copy SD → Flash
    // -----------------------------------------------------------------
    LOGI("OTA", "phase 4 – writing firmware to flash...");

    uint8_t* block_buf = (uint8_t*)malloc(OTA_BLOCK_SIZE);
    if (!block_buf) {
        LOGE("OTA", "FATAL – cannot allocate %u-byte read buffer",
                (unsigned)OTA_BLOCK_SIZE);
        fwFile.close();
        Update.abort();
        SD.end();
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }

    size_t bytes_written = 0;
    size_t last_progress_pct = 0;
    uint32_t t_start = millis();
    bool write_error = false;

    while (bytes_written < file_size) {
        // Read one block from SD
        size_t to_read = file_size - bytes_written;
        if (to_read > OTA_BLOCK_SIZE) to_read = OTA_BLOCK_SIZE;

        int bytes_read = fwFile.read(block_buf, to_read);
        if (bytes_read <= 0) {
            LOGE("OTA", "SD read failed at offset %u (read returned %d)",
                    (unsigned)bytes_written, bytes_read);
            write_error = true;
            break;
        }

        // Write block to OTA partition
        size_t written = Update.write(block_buf, bytes_read);
        if (written != static_cast<size_t>(bytes_read)) {
            LOGE("OTA", "flash write failed at offset %u (wrote %u, expected %u, error=%d)",
                    (unsigned)bytes_written, (unsigned)written,
                    (unsigned)bytes_read, Update.getError());
            write_error = true;
            break;
        }

        bytes_written += written;

        // Progress logging (every 10%)
        size_t pct = (bytes_written * 100) / file_size;
        if (pct != last_progress_pct && (pct % 10 == 0 || pct == 100)) {
            uint32_t elapsed = millis() - t_start;
            uint32_t bps = (elapsed > 0) ? (bytes_written * 1000 / elapsed) : 0;
            LOGI("OTA", "%3u%%  (%u / %u bytes, %u KB/s)",
                    (unsigned)pct,
                    (unsigned)bytes_written, (unsigned)file_size,
                    (unsigned)(bps / 1024));
            last_progress_pct = pct;
        }

        // Feed the watchdog during long writes
        esp_task_wdt_reset();
        yield();
    }

    free(block_buf);
    fwFile.close();

    // -----------------------------------------------------------------
    // Phase 5: Finalise or abort
    // -----------------------------------------------------------------
    if (write_error) {
        LOGE("OTA", "ABORTING – error during write, keeping old firmware");
        Update.abort();
        SD.end();
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }

    LOGI("OTA", "phase 5 – validating and finalising...");

    // Validate: Update.end(true) verifies the image and sets the OTA boot flag.
    // On success, the ESP32 will boot into the new firmware on next restart.
    if (!Update.end(true)) {
        LOGE("OTA", "FATAL – Update.end() FAILED: error=%d '%s'",
                Update.getError(), Update.errorString());
        Update.abort();
        SD.end();
        sdSPI.end();
        hal_sensor_spi_reinit();
        return false;
    }

    // -----------------------------------------------------------------
    // Phase 6: Success – report and reboot
    // -----------------------------------------------------------------
    uint32_t total_time = millis() - t_start;
    LOGI("OTA", "===== UPDATE SUCCESSFUL =====");
    LOGI("OTA", "wrote %u bytes in %u ms (%u KB/s)",
            (unsigned)bytes_written, (unsigned)total_time,
            (unsigned)(bytes_written * 1000 / total_time / 1024));
    LOGI("OTA", "rebooting into new firmware in 2 seconds...");

    // Clean up SD card resources (best-effort – we're rebooting anyway)
    SD.end();
    sdSPI.end();

    // Wait to let the log messages flush via Serial
    hal_delay_ms(2000);

    LOGI("OTA", "RESTARTING NOW");
    DBG.flush();
    ESP.restart();

    // Should never reach here
    return true;
}
