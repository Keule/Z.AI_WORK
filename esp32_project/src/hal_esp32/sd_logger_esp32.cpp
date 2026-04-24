/**
 * @file sd_logger_esp32.cpp
 * @brief ESP32-S3 implementation of the SD-Card Data Logger + Maintenance Task.
 *
 * Platform-specific code for the logger:
 *   - GPIO 47 switch reading (with debounce)
 *   - PSRAM ring buffer allocation (TASK-029)
 *   - FreeRTOS maintTask (lowest priority) — replaces standalone loggerTask
 *   - SD card init/deinit on shared SPI2_HOST bus
 *   - CSV file creation and writing
 *   - NTRIP connect/reconnect (TASK-029)
 *   - ETH link monitoring (TASK-029)
 *
 * SPI bus strategy:
 *   The SD card uses SPI2_HOST (SD_SPI_BUS) with pins SCK=7, MISO=5, MOSI=6.
 *   The sensor bus also uses SENS_SPI_BUS but with DIFFERENT pins (SCK=16, MISO=15, MOSI=17).
 *   During SD card access, the sensor SPI is released via hal_sensor_spi_deinit(),
 *   then SD_SPI_BUS is re-initialised with SD pins. After SD access, the sensor SPI
 *   is restored via hal_sensor_spi_reinit().
 *
 *   To prevent the control loop from accessing the bus while SD is active,
 *   a s_spi_busy flag is used:
 *     - maintTask sets flag before SD access, clears after
 *     - Control loop should check hal_spi_busy() and skip sensor reads
 *       when the flag is set (use last known values)
 *
 *   The sensor SPI is unavailable for ~50-200 ms per flush cycle.
 *
 * ESP32 Classic (T-ETH-Lite-ESP32):
 *   No sensor SPI bus switching is required for SD access.  Although the SD
 *   card and the sensor bus share the same HSPI peripheral, all sensor pins
 *   (IMU, ADS, ACT) are set to -1 in the Classic board profile — no sensor
 *   peripherals are initialised, so the SD card has exclusive use of HSPI.
 *   The SPI re-init dance (deinit/reinit) is effectively a no-op on Classic.
 *
 * TASK-029 Architecture:
 *   The maintTask replaces the standalone loggerTask and consolidates
 *   all blocking operations into one low-priority task:
 *     1. PSRAM ring buffer → SD card (every 2 s)
 *     2. NTRIP state machine / connect (every 1 s, blocking OK)
 *     3. ETH link monitoring (on change)
 */

#include "hal/hal.h"
#include "fw_config.h"
#include "logic/features.h"
#include "logic/module_interface.h"
#include "logic/sd_logger.h"
#include "logic/global_state.h"

// Forward declarations from op_mode.h — avoid including op_mode.h directly
// because it defines typedef enum OpMode which conflicts with module_interface.h's enum class OpMode
extern "C" void opModeGpioPoll(void);
extern "C" bool opModeIsPaused(void);
extern "C" bool opModeIsControlActive(void);

#include "logic/log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MAINT
#include "esp_log.h"
#include "logic/log_ext.h"

// ESP32 / Arduino includes
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>

#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
#include "logic/ntrip.h"
#endif

// ===================================================================
// Configuration
// ===================================================================

/// PSRAM ring buffer capacity (power of 2).
/// 32768 records × 32 bytes = 1 MB = ~53 minutes at 10 Hz.
#define PSRAM_RING_CAPACITY  32768

/// SD flush interval (ms). The task flushes the ring buffer to SD
/// at this interval.
static const uint32_t SD_FLUSH_INTERVAL_MS = 2000;

/// Switch debounce time (ms). After a state change, wait this long
/// before accepting the new state.
static const uint32_t SWITCH_DEBOUNCE_MS = 200;

/// Maximum CSV line length (one record formatted as CSV).
static const size_t CSV_LINE_MAX = 160;

// ===================================================================
// State
// ===================================================================

/// FreeRTOS task handle
static TaskHandle_t s_maint_task_handle = nullptr;

/// Current switch state (true = logging active)
static volatile bool s_logging_active = false;

/// SD card file handle (valid only while s_logging_active is true)
static fs::File s_log_file;

/// Current log file number (auto-incremented for each new session)
static uint32_t s_file_counter = 0;

/// SPI busy flag – set to true while SD card is accessing SPI2_HOST.
/// Sensor reads should be skipped when this is true.
static volatile bool s_spi_busy = false;

/// PSRAM ring buffer pointer (allocated in sdLoggerPsramInit)
static SdLogRecord* s_psram_ring = nullptr;

/// PSRAM buffer active flag
static bool s_psram_active = false;

/// Statistics
static uint32_t s_last_flush_count = 0;
static uint32_t s_last_overflow = 0;
static uint32_t s_session_flush_total = 0;

/// ETH link monitoring state
static bool s_last_eth_link = false;

// ===================================================================
// Platform hooks – called from sd_logger.cpp
// ===================================================================

/// Global declarations from sd_logger.cpp
extern "C" bool sdLoggerHasRecords(void);
extern "C" bool sdLoggerReadRecord(SdLogRecord* out);
extern "C" void sdLoggerIncrementFlushed(uint32_t count);
extern "C" uint32_t sdLoggerGetOverflowCount(void);

extern "C" bool sdLoggerReadSwitch(void) {
    // Active LOW: LOW = logging ON, HIGH = logging OFF
    return digitalRead(LOG_SWITCH_PIN) == LOW;
}

// ===================================================================
// SPI bus coordination
// ===================================================================

bool hal_spi_busy(void) {
    return s_spi_busy;
}

// ===================================================================
// CSV formatting
// ===================================================================

/**
 * Format a log record as a CSV line.
 *
 * CSV columns:
 *   timestamp_ms, heading_deg, steer_angle_deg, desired_angle_deg,
 *   yaw_rate_dps, roll_deg, safety_ok
 *
 * @param rec   The log record to format.
 * @param buf   Output buffer (at least CSV_LINE_MAX bytes).
 * @return      Number of characters written (excluding null terminator).
 */
static size_t formatCsvLine(const SdLogRecord& rec, char* buf) {
    int n = snprintf(buf, CSV_LINE_MAX,
        "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%u",
        (unsigned long)rec.timestamp_ms,
        (double)rec.heading_deg,
        (double)rec.steer_angle_deg,
        (double)rec.desired_angle_deg,
        (double)rec.yaw_rate_dps,
        (double)rec.roll_deg,
        (unsigned)rec.safety_ok);

    return (n > 0) ? static_cast<size_t>(n) : 0;
}

// ===================================================================
// SD card operations
// ===================================================================

/**
 * Open a new CSV log file on the SD card.
 *
 * Filename format: /log_001.csv, /log_002.csv, etc.
 * Finds the next available file number.
 *
 * @return true if the file was opened successfully.
 */
static bool openLogFile(void) {
    s_file_counter++;

    // Scan for next available file number
    char path[32];
    for (uint32_t i = s_file_counter; i < 10000; i++) {
        snprintf(path, sizeof(path), "/log_%03lu.csv", (unsigned long)i);
        if (!SD.exists(path)) {
            s_file_counter = i;
            break;
        }
    }

    snprintf(path, sizeof(path), "/log_%03lu.csv", (unsigned long)s_file_counter);
    s_log_file = SD.open(path, FILE_WRITE);

    if (!s_log_file) {
        LOGE("MAINT", "ERROR – cannot create %s", path);
        return false;
    }

    // Write CSV header
    s_log_file.println("timestamp_ms,heading_deg,steer_angle_deg,desired_angle_deg,"
                       "yaw_rate_dps,roll_deg,safety_ok");
    s_log_file.flush();

    LOGI("MAINT", "opened %s", path);
    return true;
}

/**
 * Flush the ring buffer to the SD card.
 *
 * @return Number of records written to SD.
 */
static uint32_t flushBufferToSD(void) {
    if (!s_log_file) return 0;

    char csv_buf[CSV_LINE_MAX];
    uint32_t count = 0;
    const uint32_t batch_limit = 1024;

    while (sdLoggerHasRecords() && count < batch_limit) {
        SdLogRecord rec;
        if (!sdLoggerReadRecord(&rec)) break;

        size_t len = formatCsvLine(rec, csv_buf);
        if (len > 0) {
            s_log_file.write(reinterpret_cast<uint8_t*>(csv_buf), len);
            s_log_file.write('\n');
            count++;
            // Cooperative yield to avoid starving IDLE0 on long SD batches.
            if ((count % 64u) == 0u) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }

    if (count > 0) {
        s_log_file.flush();
    }

    return count;
}

// ===================================================================
// SD card init / deinit on shared SPI bus
// ===================================================================

/**
 * Claim the SPI bus for SD card access.
 *
 * The SD card uses SD_SPI_BUS with pins SCK=7, MISO=5, MOSI=6 (CS=42).
 * The sensor bus uses SENS_SPI_BUS with pins SCK=16, MISO=15, MOSI=17.
 * We must release the sensor SPIClass before the SD library can
 * claim the SENS_SPI_BUS peripheral with its own pins.
 */
static void sdBusClaim(void) {
    s_spi_busy = true;
    hal_sensor_spi_deinit();   // release SD_SPI_BUS peripheral
    hal_delay_ms(2);
}

/**
 * Release the SPI bus back to sensor code.
 */
static void sdBusRelease(void) {
    hal_sensor_spi_reinit();   // reclaim SD_SPI_BUS peripheral for sensors
    s_spi_busy = false;
}

// ===================================================================
// PSRAM ring buffer allocation — TASK-029
// ===================================================================

/**
 * Allocate PSRAM ring buffer for logging.
 *
 * Tries to allocate from PSRAM first (~1 MB), falls back to regular
 * heap, and finally falls back to the static buffer in sd_logger.cpp.
 *
 * The buffer selection happens ONCE at init — there is no runtime
 * fallback after the first successful allocation.  If PSRAM allocation
 * fails at init, the static 16 KB buffer is used for the entire session.
 *
 * NOTE: The SD_SPI_BUS bus is shared with the sensor SPI.  Each SD flush cycle
 * requires hal_sensor_spi_deinit()/hal_sensor_spi_reinit(), which blocks
 * the sensor SPI for ~50-200 ms.  This is NOT fully decoupled — the
 * control loop must skip sensor reads while s_spi_busy is set.
 *
 * @return true if a large buffer (PSRAM or heap) was allocated.
 */
static bool sdLoggerPsramInit(void) {
    const size_t buf_size = PSRAM_RING_CAPACITY * sizeof(SdLogRecord);

    // Try PSRAM first
    s_psram_ring = static_cast<SdLogRecord*>(
        heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM));

    if (s_psram_ring) {
        LOGI("MAINT", "PSRAM ring buffer allocated: %u KB (%u records)",
             (unsigned)(buf_size / 1024),
             (unsigned)PSRAM_RING_CAPACITY);
    } else {
        // Fallback: try regular heap
        LOGW("MAINT", "PSRAM alloc failed (%u bytes), trying regular heap",
             (unsigned)buf_size);
        s_psram_ring = static_cast<SdLogRecord*>(malloc(buf_size));
    }

    if (!s_psram_ring) {
        LOGW("MAINT", "heap alloc also failed, using static 16 KB buffer");
        s_psram_active = false;
        return false;
    }

    // Redirect the ring buffer in sd_logger.cpp to use this buffer
    sdLoggerSetExternalBuffer(s_psram_ring, PSRAM_RING_CAPACITY);
    s_psram_active = true;
    return true;
}

// ===================================================================
// ETH link monitoring — TASK-029
// ===================================================================

/**
 * Monitor ETH link status and log/report on state changes.
 */
static void maintEthMonitor(void) {
    bool eth_link = hal_net_is_connected();

    if (eth_link != s_last_eth_link) {
        s_last_eth_link = eth_link;
        if (eth_link) {
            LOGI("MAINT", "ETH link UP");
        } else {
            LOGW("MAINT", "ETH link DOWN");
        }
    }
}

// ===================================================================
// Maintenance Task — TASK-029 (replaces standalone loggerTask)
// ===================================================================

/**
 * Main function for the maintenance FreeRTOS task.
 *
 * Runs at LOWEST priority on Core 0, 8 KB stack.
 * Handles three responsibilities:
 *
 *   1. SD card logging:
 *      - Checks hardware switch every 1 s
 *      - Flushes ring buffer to SD every 2 s
 *      - Open/close log file on switch transitions
 *
 *   2. NTRIP connect/reconnect:
 *      - Runs ntripTick() state machine every 1 s
 *      - Blocking TCP connect is OK here (lowest priority)
 *
 *   3. ETH link monitoring:
 *      - Checks link status every 1 s, logs on changes
 *
 * For each SD flush cycle:
 *   1. Claim SPI bus (set s_spi_busy flag)
 *   2. Init SD card on SD_SPI_BUS with SD pins
 *   3. Open/append log file, write records
 *   4. Close file, release SD
 *   5. Release SPI bus (clear s_spi_busy flag)
 *
 * We open/close the file on each flush to minimise bus hold time.
 * The overhead is minimal (~100 ms total) and guarantees the sensor
 * SPI is available between flushes.
 */
static void maintTaskFunc(void* param) {
    (void)param;
    LOGI("MAINT", "task started on core %d (priority=1, stack=8 KB)", xPortGetCoreID());

    bool was_active = false;
    uint32_t last_switch_change_ms = 0;
    bool last_switch_raw = sdLoggerReadSwitch();
    uint32_t loop_count = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 s base interval
        esp_task_wdt_reset();  // Feed TWDT every iteration — prevents IDLE0 starvation
        loop_count++;

        // ADR-005: GPIO-Poll fuer Modus-Switch
        opModeGpioPoll();

        // -----------------------------------------------------------------
        // 1. ETH link monitoring (every iteration = 1 s)
        // -----------------------------------------------------------------
        maintEthMonitor();

        // -----------------------------------------------------------------
        // 2. NTRIP state machine (every iteration = 1 s)
        //     Only connects in ACTIVE mode.  Feeds WDT around hal_tcp_connect()
        //     which can block for up to 3 s and would starve the IDLE task.
        // -----------------------------------------------------------------
#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
        // NTRIP only runs when control is ACTIVE (not BOOTING, not PAUSED).
        // Feed WDT before/after ntripTick() since hal_tcp_connect() can
        // block for up to 3 seconds and starve the IDLE task on CPU 0.
        if (!opModeIsControlActive()) {
            // BOOTING or PAUSED — do not attempt NTRIP connections.
            if (hal_tcp_connected()) {
                LOGI("MAINT", "NTRIP disconnect (control not active)");
                hal_tcp_disconnect();
            }
        } else {
            esp_task_wdt_reset();  // Feed WDT before potentially blocking call
            ntripTick();
            esp_task_wdt_reset();  // Feed WDT after potentially blocking call
        }
#endif

        // -----------------------------------------------------------------
        // 3. SD card logging (every 2nd iteration = 2 s)
        // -----------------------------------------------------------------
        if (!moduleSysIsActive(ModuleId::LOGGING)) {
            if (s_logging_active || was_active) {
                LOGW("MAINT", "MOD_SD inactive -> forcing logger idle");
                s_logging_active = false;
                was_active = false;
            }
            continue;
        }

        // Read switch with simple debounce. If LOGSW module is not active,
        // force logger switch OFF to avoid phantom SD sessions.
        const bool logsw_active = moduleSysIsActive(ModuleId::LOGGING);  // MOD_LOGSW mapped to LOGGING
        bool switch_raw = logsw_active ? sdLoggerReadSwitch() : false;
        uint32_t now = millis();
        if (switch_raw != last_switch_raw) {
            last_switch_change_ms = now;
            last_switch_raw = switch_raw;
        }
        bool switch_debounced = switch_raw;

        // State machine: switch ON -> start logging, switch OFF -> stop
        if (switch_debounced && !was_active) {
            // --- TRANSITION: OFF -> ON ---
            LOGI("MAINT", "log switch ON – starting logging session");

            esp_task_wdt_reset();  // Feed WDT before blocking SD init
            sdBusClaim();

            // Init SD card on shared SPI2_HOST
            SPIClass sdSPI(SD_SPI_BUS);
            sdSPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS);

            if (SD.begin(SD_CS, sdSPI, 4000000, "/sd", 2)) {
                LOGI("MAINT", "SD card mounted OK");
                openLogFile();
                s_logging_active = true;
                s_session_flush_total = 0;
                s_last_flush_count = 0;
                s_last_overflow = 0;
            } else {
                LOGE("MAINT", "ERROR – SD card init failed");
                s_logging_active = false;
            }

            SD.end();
            sdSPI.end();

            sdBusRelease();
            esp_task_wdt_reset();  // Feed WDT after blocking SD init
            was_active = true;

        } else if (!switch_debounced && was_active) {
            // --- TRANSITION: ON -> OFF ---
            LOGI("MAINT", "log switch OFF – stopping logging session");
            LOGI("MAINT", "session stats: %lu records flushed, %lu buffer overflows",
                    (unsigned long)s_session_flush_total,
                    (unsigned long)sdLoggerGetOverflowCount() - s_last_overflow);

            s_logging_active = false;
            was_active = false;

        } else if (switch_debounced && was_active && (loop_count % 2 == 0)) {
            // --- ACTIVE: flush ring buffer to SD (every 2 s) ---
            if (!sdLoggerHasRecords()) continue;

            esp_task_wdt_reset();  // Feed WDT before blocking SD flush
            sdBusClaim();

            // Re-init SD (we close it after each flush)
            SPIClass sdSPI(SD_SPI_BUS);
            sdSPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS);

            if (SD.begin(SD_CS, sdSPI, 4000000, "/sd", 2)) {
                // Re-open the log file in append mode
                char path[32];
                snprintf(path, sizeof(path), "/log_%03lu.csv", (unsigned long)s_file_counter);
                s_log_file = SD.open(path, FILE_APPEND);

                if (s_log_file) {
                    uint32_t flushed = flushBufferToSD();
                    sdLoggerIncrementFlushed(flushed);
                    s_session_flush_total += flushed;

                    if (flushed > 0) {
                        uint32_t buf_remaining = sdLoggerGetBufferCount();
                        uint32_t overflows = sdLoggerGetOverflowCount();
                        LOGI("MAINT", "SD flushed %lu records (buf=%lu, overflow=%lu)",
                                (unsigned long)flushed,
                                (unsigned long)buf_remaining,
                                (unsigned long)overflows);
                    }

                    s_log_file.close();
                }
            } else {
                LOGE("MAINT", "ERROR – SD card re-init failed during flush");
            }

            SD.end();
            sdSPI.end();

            sdBusRelease();
            esp_task_wdt_reset();  // Feed WDT after blocking SD flush
        }
        // else: switch OFF and was_active=false -> nothing to do (idle)
    }
}

// ===================================================================
// Public API – ESP32 implementation
// ===================================================================

void sdLoggerInit(void) {
    if (!moduleSysIsActive(ModuleId::LOGGING)) {
        LOGW("MAINT", "MOD_SD inactive -> skip legacy logger init");
        s_logging_active = false;
        return;
    }

    // Legacy init — uses static 16 KB ring buffer, creates standalone loggerTask.
    // Configure logging switch GPIO
    pinMode(LOG_SWITCH_PIN, INPUT_PULLUP);
    LOGI("MAINT", "legacy init: switch on GPIO %d (active LOW)", LOG_SWITCH_PIN);

    s_logging_active = false;

    // Create the legacy logger task on Core 0 with LOWEST priority.
    xTaskCreatePinnedToCore(
        maintTaskFunc,
        "logger",
        4096,
        nullptr,
        1,              // LOWEST priority
        &s_maint_task_handle,
        0               // Core 0
    );

    LOGI("MAINT", "legacy logger initialised (flush interval = 2000 ms, static 16 KB buffer)");
}

void sdLoggerMaintInit(void) {
    // TASK-029 init — PSRAM buffer + combined maintTask.
    // Configure logging switch GPIO
    if (moduleSysIsActive(ModuleId::LOGGING)) {  // was: MOD_SD && MOD_LOGSW
        pinMode(LOG_SWITCH_PIN, INPUT_PULLUP);
        LOGI("MAINT", "switch on GPIO %d (active LOW)", LOG_SWITCH_PIN);
    } else {
        LOGW("MAINT", "LOGGING module inactive -> SD logging switch disabled");
    }

    s_logging_active = false;

    // Allocate PSRAM ring buffer
    bool psram_ok = moduleSysIsActive(ModuleId::LOGGING) ? sdLoggerPsramInit() : false;

    // Create the maintenance task on Core 0 with LOWEST priority.
    xTaskCreatePinnedToCore(
        maintTaskFunc,
        "maint",
        8192,           // 8 KB stack (larger: NTRIP + SD + ETH)
        nullptr,
        1,              // LOWEST priority
        &s_maint_task_handle,
        0               // Core 0
    );

    if (psram_ok) {
        LOGI("MAINT", "initialised (PSRAM buffer, SD+NTRIP+ETH)");
    } else if (moduleSysIsActive(ModuleId::LOGGING)) {
        LOGI("MAINT", "initialised (static buffer fallback, SD+NTRIP+ETH)");
    } else {
        LOGI("MAINT", "initialised (NTRIP+ETH maintenance, SD logging disabled)");
    }
}

// sdLoggerPsramBufferActive() and sdLoggerPsramBufferCount() are
// defined in sd_logger.cpp — they check s_ring_buf != s_ring_buf_static.
// No ESP32-specific duplicate needed here.
