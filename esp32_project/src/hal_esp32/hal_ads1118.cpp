/**
 * @file hal_ads1118.cpp
 * @brief ADS1118 16-Bit ADC for steering angle potentiometer.
 *
 * Domain: ADS1118 / Steering Angle Sensor
 * Split from hal_impl.cpp (ADR-HAL-002).
 *
 * Uses libdriver/ads1118 (lib/ads1118/src/) for initialisation only.
 * For runtime reads, we bypass the libdriver and read raw ADC data
 * directly via SPI to avoid the "range is invalid" bug in the
 * libdriver's continuous_read() (which reads back the config register
 * and fails if SPI returns garbage).
 *
 * Wiring:
 *   ADS1118 DOUT  -> GPIO 21 (MISO)
 *   ADS1118 DIN   -> GPIO 38 (MOSI)
 *   ADS1118 SCLK  -> GPIO 47 (SCK)
 *   ADS1118 CS    -> GPIO 18
 *
 * Calibration:
 *   Raw ADC min (left stop)  -> -22.5°
 *   Raw ADC max (right stop) -> +22.5°
 *   Stored in NVS (Preferences) and survives reboots.
 */

#include "hal/hal.h"
#include "hal_esp32_internal.h"
#include "fw_config.h"
#include "logic/log_config.h"
#include "logic/log_ext.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_HAL
#include "esp_log.h"

#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include "driver_ads1118.h"
#include <cstring>
#include <cstdarg>

// ===================================================================
// ADS1118 handle and state
// ===================================================================

/// libdriver ADS1118 handle (used for init/config only)
static ads1118_handle_t s_ads1118_handle;

/// ADS1118 detected flag
static bool s_ads1118_detected = false;

/// Calibration state
static bool   s_calibrated = false;
static int16_t s_cal_left_raw  = 0;   // ADC value at left stop  -> -22.5°
static int16_t s_cal_right_raw = 0;   // ADC value at right stop -> +22.5°

/// NVS namespace and keys for calibration persistence
static const char* NVS_NAMESPACE = "agsteer";
static const char* NVS_KEY_CAL_VALID  = "cal_v";
static const char* NVS_KEY_CAL_LEFT   = "cal_l";
static const char* NVS_KEY_CAL_RIGHT  = "cal_r";
static constexpr uint32_t NVS_CAL_MAGIC = 0xA6511C00;  // magic to validate stored data

// ===================================================================
// libdriver interface functions (static, ESP32-specific)
// ===================================================================

static uint8_t ads1118_if_spi_init(void) {
    pinMode(CS_STEER_ANG, OUTPUT);
    digitalWrite(CS_STEER_ANG, HIGH);
    return 0;
}

static uint8_t ads1118_if_spi_deinit(void) {
    return 0;  // Don't deinit shared bus
}

static uint8_t ads1118_if_spi_transmit(uint8_t *tx, uint8_t *rx, uint16_t len) {
    hal_esp32_ads_spi_transfer(tx, rx, len);
    return 0;
}

static void ads1118_if_delay_ms(uint32_t ms) {
    delay(ms);
}

static void ads1118_if_debug_print(const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[256];
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
}

// ===================================================================
// Direct raw ADC read (bypasses libdriver, uses SPI cache from hal_spi.cpp)
// ===================================================================

/// Read one raw 16-bit ADC sample (cached, deadline-checked via SPI telemetry).
/// Delegates to hal_esp32_ads_read_raw_cached() in hal_spi.cpp.
static int16_t ads1118_read_raw(void) {
    return hal_esp32_ads_read_raw_cached();
}

/// Read multiple raw samples and return the median value.
/// More robust than average against outliers.
/// @param samples  number of samples to read (should be odd for true median)
/// @param sample_delay_ms delay between samples in ms (default: 5ms for 250 SPS)
static int16_t ads1118_read_raw_median(int samples, int sample_delay_ms = 5) {
    // Buffer for samples (max 31, enough for median)
    int16_t buf[31];
    if (samples > 31) samples = 31;
    if (samples < 1) samples = 1;

    for (int i = 0; i < samples; i++) {
        buf[i] = ads1118_read_raw();
        if (sample_delay_ms > 0) delay(sample_delay_ms);
    }

    // Simple bubble sort for small array
    for (int i = 0; i < samples - 1; i++) {
        for (int j = i + 1; j < samples; j++) {
            if (buf[j] < buf[i]) {
                int16_t tmp = buf[i];
                buf[i] = buf[j];
                buf[j] = tmp;
            }
        }
    }

    return buf[samples / 2];  // median
}

// ===================================================================
// Calibration persistence (NVS)
// ===================================================================

/// Load calibration from NVS flash.
static bool steer_cal_load(void) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // read-only mode
        hal_log("SteerCal: NVS open failed");
        return false;
    }

    uint32_t magic = prefs.getUInt(NVS_KEY_CAL_VALID, 0);
    if (magic != NVS_CAL_MAGIC) {
        prefs.end();
        hal_log("SteerCal: no valid calibration in NVS");
        return false;
    }

    s_cal_left_raw  = static_cast<int16_t>(prefs.getInt(NVS_KEY_CAL_LEFT, 0));
    s_cal_right_raw = static_cast<int16_t>(prefs.getInt(NVS_KEY_CAL_RIGHT, 0));
    s_calibrated = true;

    prefs.end();

    hal_log("SteerCal: loaded from NVS (left=%d, right=%d)",
            s_cal_left_raw, s_cal_right_raw);
    return true;
}

/// Save calibration to NVS flash.
static void steer_cal_save(void) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {  // read-write mode
        hal_log("SteerCal: NVS open failed, cannot save!");
        return;
    }

    prefs.putUInt(NVS_KEY_CAL_VALID, NVS_CAL_MAGIC);
    prefs.putInt(NVS_KEY_CAL_LEFT, static_cast<int32_t>(s_cal_left_raw));
    prefs.putInt(NVS_KEY_CAL_RIGHT, static_cast<int32_t>(s_cal_right_raw));

    prefs.end();

    hal_log("SteerCal: saved to NVS (left=%d, right=%d)",
            s_cal_left_raw, s_cal_right_raw);
}

/// Wait for user to press Enter on Serial monitor.
/// While waiting, shows live ADC voltage at 20 Hz (every 50ms).
static void wait_for_enter_live_adc(void) {
    uint32_t last_print = 0;
    while (true) {
        if (Serial.available()) {
            int c = Serial.read();
            if (c == '\n' || c == '\r') {
                while (Serial.available()) Serial.read();
                int16_t raw = ads1118_read_raw();
                float voltage = raw * 4.096f / 32768.0f;
                Serial.printf("   -> %7.3f V  (raw=%d)\n", voltage, raw);
                delay(50);
                return;
            }
        }
        uint32_t now = millis();
        if (now - last_print >= 50) {
            last_print = now;
            int16_t raw = ads1118_read_raw();
            float voltage = raw * 4.096f / 32768.0f;
            Serial.printf("   -> %7.3f V  (raw=%d)  \r", voltage, raw);
        }
        delay(2);
    }
}

// ===================================================================
// Public API — Steering Angle (hal.h)
// ===================================================================

void hal_steer_angle_begin(void) {
    // Ensure all other SPI device CS pins are configured as outputs
    if (CS_IMU >= 0) { pinMode(CS_IMU, OUTPUT); digitalWrite(CS_IMU, HIGH); }
    if (CS_ACT >= 0) { pinMode(CS_ACT, OUTPUT); digitalWrite(CS_ACT, HIGH); }

    // Wire up libdriver handle with ESP32 interface functions
    DRIVER_ADS1118_LINK_INIT(&s_ads1118_handle, ads1118_handle_t);
    DRIVER_ADS1118_LINK_SPI_INIT(&s_ads1118_handle, ads1118_if_spi_init);
    DRIVER_ADS1118_LINK_SPI_DEINIT(&s_ads1118_handle, ads1118_if_spi_deinit);
    DRIVER_ADS1118_LINK_SPI_TRANSMIT(&s_ads1118_handle, ads1118_if_spi_transmit);
    DRIVER_ADS1118_LINK_DELAY_MS(&s_ads1118_handle, ads1118_if_delay_ms);
    DRIVER_ADS1118_LINK_DEBUG_PRINT(&s_ads1118_handle, ads1118_if_debug_print);

    // Initialise driver
    uint8_t res = ads1118_init(&s_ads1118_handle);
    if (res != 0) {
        hal_log("ESP32: ADS1118 init failed (err=%u)", res);
        return;
    }

    // Configure: AIN0 single-ended, +/-4.096V, 250 SPS, ADC mode
    ads1118_set_channel(&s_ads1118_handle, ADS1118_CHANNEL_AIN0_GND);
    ads1118_set_range(&s_ads1118_handle, ADS1118_RANGE_4P096V);
    ads1118_set_rate(&s_ads1118_handle, ADS1118_RATE_250SPS);
    ads1118_set_mode(&s_ads1118_handle, ADS1118_MODE_ADC);

    // Try to load calibration from NVS
    steer_cal_load();

    hal_log("ESP32: ADS1118 configured (AIN0, +/-4.096V, 250 SPS, calibrated=%s)",
            s_calibrated ? "YES" : "NO");
}

bool hal_steer_angle_detect(void) {
    int16_t raw1 = 0;
    int16_t raw2 = 0;
    bool looks_valid = false;

    for (int attempt = 0; attempt < 3 && !looks_valid; attempt++) {
        if (attempt > 0) {
            hal_log("ESP32: ADS1118 detect retry %d/2...", attempt);
            delay(100);
        }

        raw1 = ads1118_read_raw();
        delay(20);
        raw2 = ads1118_read_raw();

        looks_valid = (raw1 != 0 && raw1 != -1 && raw1 != 0x7FFF &&
                       raw2 != 0 && raw2 != -1 && raw2 != 0x7FFF);
    }

    // If still failing, try a full ADS1118 re-init (write config register)
    if (!looks_valid) {
        hal_log("ESP32: ADS1118 still 0x%04X after retries, re-initialising chip...", raw1);
        uint8_t tx[4] = {0x84, 0xC3, 0xFF, 0xFF};
        uint8_t rx[4];
        ads1118_if_spi_transmit(tx, rx, 4);
        delay(50);

        raw1 = ads1118_read_raw();
        delay(20);
        raw2 = ads1118_read_raw();
        looks_valid = (raw1 != 0 && raw1 != -1 && raw1 != 0x7FFF &&
                       raw2 != 0 && raw2 != -1 && raw2 != 0x7FFF);
        hal_log("ESP32: ADS1118 after re-init: raw1=%d (0x%04X) raw2=%d (0x%04X) %s",
                raw1, (unsigned)raw1 & 0xFFFF, raw2, (unsigned)raw2 & 0xFFFF,
                looks_valid ? "OK" : "STILL FAIL");
    }

    s_ads1118_detected = looks_valid;

    if (s_ads1118_detected) {
        hal_log("ESP32: ADS1118 DETECTED (raw1=%d, raw2=%d)", raw1, raw2);

        uint8_t res = ads1118_start_continuous_read(&s_ads1118_handle);
        if (res != 0) {
            hal_log("ESP32: ADS1118 continuous start note: err=%u (using direct reads)", res);
        } else {
            hal_log("ESP32: ADS1118 continuous mode started");
        }
    } else {
        hal_log("ESP32: ADS1118 DETECT FAILED (raw1=0x%04X, raw2=0x%04X)",
                (unsigned)raw1 & 0xFFFF, (unsigned)raw2 & 0xFFFF);
    }

    return s_ads1118_detected;
}

void hal_steer_angle_calibrate(void) {
    if (!s_ads1118_detected) {
        hal_log("SteerCal: ERROR — ADS1118 not detected, cannot calibrate");
        return;
    }

    hal_log("========================================");
    hal_log("  STEERING ANGLE CALIBRATION");
    hal_log("========================================");
    Serial.println();
    Serial.println("=== Lenkwinkel Kalibrierung ===");
    Serial.println();

    // --- LEFT STOP ---
    Serial.println("1) Lenkung ganz nach LINKS fahren (linker Anschlag)");
    Serial.println("   ENTER druecken zum Speichern des Wertes:");
    Serial.flush();
    wait_for_enter_live_adc();

    int16_t left_val = ads1118_read_raw_median(11, 8);
    float v_left = left_val * 4.096f / 32768.0f;
    hal_log("SteerCal: left stop  -> raw=%d, %.3f V", left_val, v_left);
    Serial.printf("   Gespeichert: %7.3f V  (raw=%d)\n", v_left, left_val);
    Serial.println();

    // --- RIGHT STOP ---
    Serial.println("2) Lenkung ganz nach RECHTS fahren (rechter Anschlag)");
    Serial.println("   ENTER druecken zum Speichern des Wertes:");
    Serial.flush();
    wait_for_enter_live_adc();

    int16_t right_val = ads1118_read_raw_median(11, 8);
    float v_right = right_val * 4.096f / 32768.0f;
    hal_log("SteerCal: right stop -> raw=%d, %.3f V", right_val, v_right);
    Serial.printf("   Gespeichert: %7.3f V  (raw=%d)\n", v_right, right_val);
    Serial.println();

    // --- Validate ---
    if (left_val == right_val) {
        hal_log("SteerCal: ERROR — left == right (%d), no steering range!", left_val);
        Serial.println("FEHLER: Links und Rechts sind gleich! Nochmal versuchen.");
        Serial.println();
        return;
    }

    // Ensure left < right (swap if poti is wired in reverse)
    if (left_val > right_val) {
        int16_t tmp = left_val;
        left_val = right_val;
        right_val = tmp;
        hal_log("SteerCal: values swapped (left > right), poti wiring reversed");
        Serial.println("   Hinweis: Poti polaritaet automatisch korrigiert");
    }

    s_cal_left_raw = left_val;
    s_cal_right_raw = right_val;
    s_calibrated = true;

    steer_cal_save();

    int16_t span = right_val - left_val;
    float voltage_left  = left_val  * 4.096f / 32768.0f;
    float voltage_right = right_val * 4.096f / 32768.0f;

    Serial.println("=== Kalibrierung abgeschlossen ===");
    Serial.printf("   Links:  raw=%6d  (%.3f V)  -> -45.0°\n", left_val, voltage_left);
    Serial.printf("   Rechts: raw=%6d  (%.3f V)  -> +45.0°\n", right_val, voltage_right);
    Serial.printf("   Spanne: %d LSB  (%.3f V)\n", span, voltage_right - voltage_left);
    Serial.println();
    Serial.println("Werte im Flash gespeichert. Kalibrierung ueberlebt Neustart.");
    Serial.println("========================================");
    Serial.println();
}

float hal_steer_angle_read_deg(void) {
    if (!s_ads1118_detected || !s_calibrated) {
        return 0.0f;
    }

    int16_t raw = ads1118_read_raw();

    int16_t span = s_cal_right_raw - s_cal_left_raw;
    if (span == 0) return 0.0f;

    float normalised = static_cast<float>(raw - s_cal_left_raw) / static_cast<float>(span);

    if (normalised < 0.0f) normalised = 0.0f;
    if (normalised > 1.0f) normalised = 1.0f;

    float angle = (normalised * 45.0f) - 22.5f;

    return angle;
}

int16_t hal_steer_angle_read_raw(void) {
    if (!s_ads1118_detected || !s_calibrated) return 0;
    return ads1118_read_raw();
}

uint8_t hal_steer_angle_read_sensor_byte(void) {
    if (!s_ads1118_detected) return 0;

    const int16_t raw = ads1118_read_raw();
    const int16_t span = s_cal_right_raw - s_cal_left_raw;
    if (s_calibrated && span != 0) {
        float normalised = static_cast<float>(raw - s_cal_left_raw) / static_cast<float>(span);
        if (normalised < 0.0f) normalised = 0.0f;
        if (normalised > 1.0f) normalised = 1.0f;
        return static_cast<uint8_t>((normalised * 255.0f) + 0.5f);
    }

    return static_cast<uint8_t>(raw & 0xFF);
}

bool hal_steer_angle_is_calibrated(void) {
    return s_calibrated;
}
