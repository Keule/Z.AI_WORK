/**
 * @file hal_bno085.cpp
 * @brief BNO085 HAL binding using the 7Semi BNO08x SPI example pattern.
 */

#include "hal/hal.h"
#include "hal_esp32_internal.h"
#include "fw_config.h"

#include <7Semi_BNO08x.h>
#include <Arduino.h>
#include <BnoSPIBus.h>
#include <SPI.h>

#include "logic/log_config.h"

#ifndef LOG_IMU_DIAG_INTERVAL_MS
#define LOG_IMU_DIAG_INTERVAL_MS 500
#endif

namespace {

constexpr uint32_t kBno085SpiHz = 6000000UL;
constexpr uint16_t kBno085GamePeriodMs = 10;
constexpr uint16_t kBno085GeoPeriodMs = 50;
constexpr uint32_t kHeadingBootMs = 2500;
constexpr uint16_t kHeadingBootMinSamples = 10;
constexpr float kRadToDeg = 57.2957795f;
constexpr float kDegToRad = 0.0174532925f;

BnoSPIBus* s_bno_bus = nullptr;
BNO08x_7Semi* s_bno08x = nullptr;

bool s_bno08x_begin_attempted = false;
bool s_bno08x_ready = false;
bool s_reports_enabled = false;
bool s_imu_cache_valid = false;
float s_imu_yaw_cache = 0.0f;
float s_imu_roll_cache = 0.0f;
float s_imu_heading_cache = 0.0f;
float s_game_yaw_cache = 0.0f;
float s_geo_yaw_cache = 0.0f;
float s_imu_acc_x_cache = 0.0f;
float s_imu_acc_y_cache = 0.0f;
float s_imu_acc_z_cache = 0.0f;
uint32_t s_imu_last_data_us = 0;
uint32_t s_heading_boot_start_ms = 0;
uint32_t s_heading_last_us = 0;
uint32_t s_last_diag_log_ms = 0;
uint32_t s_diag_read_ok = 0;
uint32_t s_diag_read_wait = 0;
uint32_t s_spi_recover_count = 0;
uint32_t s_diag_packet_count = 0;
uint16_t s_heading_boot_samples = 0;
float s_heading_boot_sin = 0.0f;
float s_heading_boot_cos = 0.0f;
float s_heading_boot_mag_deg = 0.0f;
float s_heading_boot_game_deg = 0.0f;
bool s_game_yaw_valid = false;
bool s_geo_yaw_valid = false;
bool s_geo_yaw_new = false;
bool s_heading_ready = false;

inline bool pinValid(int pin) { return pin >= 0; }

inline int safeDigitalRead(int pin, int fallback = 0) {
    return pinValid(pin) ? digitalRead(pin) : fallback;
}

void prepareChipSelects() {
    if (pinValid(CS_STEER_ANG)) {
        pinMode(CS_STEER_ANG, OUTPUT);
        digitalWrite(CS_STEER_ANG, HIGH);
    }
    if (pinValid(CS_ACT)) {
        pinMode(CS_ACT, OUTPUT);
        digitalWrite(CS_ACT, HIGH);
    }
    if (pinValid(CS_IMU)) {
        pinMode(CS_IMU, OUTPUT);
        digitalWrite(CS_IMU, HIGH);
    }

    if (pinValid(IMU_WAKE)) {
        pinMode(IMU_WAKE, OUTPUT);
        digitalWrite(IMU_WAKE, HIGH);
    }
}

bool enableRuntimeReports() {
    if (!s_bno08x) return false;

    for (uint8_t i = 0; i < 5; i++) {
        s_bno08x->processData();
        delay(10);
    }

    const bool geo_ok = s_bno08x->enableGeoRotationVector(kBno085GeoPeriodMs);
    const bool game_ok = s_bno08x->enableGameRotationVector(kBno085GamePeriodMs);

    // Some firmware replies do not match the library's SetFeature ACK parser.
    // Keep polling after requests; actual data freshness decides runtime quality.
    s_reports_enabled = true;
    s_imu_cache_valid = false;
    s_game_yaw_valid = false;
    s_geo_yaw_valid = false;
    s_geo_yaw_new = false;
    s_heading_ready = false;
    s_heading_boot_start_ms = millis();
    s_heading_boot_samples = 0;
    s_heading_boot_sin = 0.0f;
    s_heading_boot_cos = 0.0f;
    s_imu_last_data_us = 0;
    s_heading_last_us = 0;
    hal_log("ESP32: 7Semi BNO085 reports requested geo=%s/%ums game=%s/%ums poll=%s (boot_heading=%lums/%u samples)",
            geo_ok ? "OK" : "NOACK",
            (unsigned)kBno085GeoPeriodMs,
            game_ok ? "OK" : "NOACK",
            (unsigned)kBno085GamePeriodMs,
            s_reports_enabled ? "ON" : "OFF",
            (unsigned long)kHeadingBootMs,
            (unsigned)kHeadingBootMinSamples);
    return s_reports_enabled;
}

void recoverAfterSpiReinit() {
    if (!s_bno08x_begin_attempted || !s_bno08x) return;

    prepareChipSelects();
    delay(20);

    s_bno08x_ready = s_bno08x->begin();
    s_reports_enabled = false;
    s_imu_cache_valid = false;
    s_imu_last_data_us = 0;
    s_spi_recover_count++;

    hal_log("ESP32: 7Semi BNO085 SPI recovery #%lu begin %s (levels[int=%d rst=%d wake=%d])",
            (unsigned long)s_spi_recover_count,
            s_bno08x_ready ? "OK" : "FAIL",
            safeDigitalRead(IMU_INT),
            safeDigitalRead(IMU_RST),
            safeDigitalRead(IMU_WAKE));

    if (s_bno08x_ready) {
        enableRuntimeReports();
    }
}

float rollDegFromQuaternion(float i, float j, float k, float real) {
    const float norm = sqrtf((real * real) + (i * i) + (j * j) + (k * k));
    if (norm <= 0.0f) return 0.0f;

    real /= norm;
    i /= norm;
    j /= norm;
    k /= norm;

    const float t0 = 2.0f * ((real * i) + (j * k));
    const float t1 = 1.0f - (2.0f * ((i * i) + (j * j)));
    return atan2f(t0, t1) * kRadToDeg;
}

float normalize360(float deg) {
    while (deg >= 360.0f) deg -= 360.0f;
    while (deg < 0.0f) deg += 360.0f;
    return deg;
}

float deltaDeg(float current, float reference) {
    float d = current - reference;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

float yawDegFromQuaternion(float i, float j, float k, float real) {
    const float norm = sqrtf((real * real) + (i * i) + (j * j) + (k * k));
    if (norm <= 0.0f) return 0.0f;

    real /= norm;
    i /= norm;
    j /= norm;
    k /= norm;

    const float t0 = 2.0f * ((real * k) + (i * j));
    const float t1 = 1.0f - (2.0f * ((j * j) + (k * k)));
    return normalize360(atan2f(t0, t1) * kRadToDeg);
}

void updateHeadingBootstrap(uint32_t now_ms) {
    if (s_heading_ready || !s_geo_yaw_valid || !s_geo_yaw_new || !s_game_yaw_valid) return;
    s_geo_yaw_new = false;

    const float geo_rad = s_geo_yaw_cache * kDegToRad;
    s_heading_boot_sin += sinf(geo_rad);
    s_heading_boot_cos += cosf(geo_rad);
    s_heading_boot_samples++;

    const bool enough_time = (now_ms - s_heading_boot_start_ms) >= kHeadingBootMs;
    const bool enough_samples = s_heading_boot_samples >= kHeadingBootMinSamples;
    if (enough_time && enough_samples) {
        s_heading_boot_mag_deg = normalize360(atan2f(s_heading_boot_sin, s_heading_boot_cos) * kRadToDeg);
        s_heading_boot_game_deg = s_game_yaw_cache;
        s_imu_heading_cache = s_heading_boot_mag_deg;
        s_heading_ready = true;
        hal_log("IMU-HEADING: boot lock mag=%.1f deg game0=%.1f deg samples=%u window=%lums",
                s_heading_boot_mag_deg,
                s_heading_boot_game_deg,
                (unsigned)s_heading_boot_samples,
                (unsigned long)(now_ms - s_heading_boot_start_ms));
    }
}

} // namespace

void hal_imu_begin(void) {
    if (s_bno08x_begin_attempted) {
        return;
    }
    s_bno08x_begin_attempted = true;
    prepareChipSelects();
    hal_imu_set_spi_config(kBno085SpiHz, SPI_MODE3);

    SPIClass& imu_spi = hal_esp32_sensor_spi_port();
    static BnoSPIBus bus(
        imu_spi,
        CS_IMU,
        IMU_INT,
        IMU_RST,
        kBno085SpiHz,
        SPI_MODE3,
        SENS_SPI_SCK,
        SENS_SPI_MISO,
        SENS_SPI_MOSI);
    static BNO08x_7Semi bno(bus);

    s_bno_bus = &bus;
    s_bno08x = &bno;

    s_bno08x_ready = s_bno08x->begin();
    hal_log("ESP32: 7Semi BNO085 example-style begin %s (SCK=%d MISO=%d MOSI=%d CS=%d INT=%d RST=%d WAKE/PS0=%d freq=%luHz mode=3 levels[int=%d rst=%d wake=%d])",
            s_bno08x_ready ? "OK" : "FAIL",
            SENS_SPI_SCK,
            SENS_SPI_MISO,
            SENS_SPI_MOSI,
            CS_IMU,
            IMU_INT,
            IMU_RST,
            IMU_WAKE,
            (unsigned long)kBno085SpiHz,
            safeDigitalRead(IMU_INT),
            safeDigitalRead(IMU_RST),
            safeDigitalRead(IMU_WAKE));

    if (s_bno08x_ready) {
        enableRuntimeReports();
    }
}

void hal_imu_reset_pulse(uint32_t low_ms, uint32_t settle_ms) {
    prepareChipSelects();
    if (!pinValid(IMU_RST) || !pinValid(IMU_INT)) {
        hal_log("ESP32: IMU reset pulse skipped (IMU_RST or IMU_INT not mapped)");
        return;
    }
    pinMode(IMU_RST, OUTPUT);
    pinMode(IMU_INT, INPUT_PULLUP);

    const int int_before = safeDigitalRead(IMU_INT);
    const int wake_before = safeDigitalRead(IMU_WAKE);
    digitalWrite(IMU_RST, LOW);
    delay(low_ms);
    const int int_during = safeDigitalRead(IMU_INT);
    const int rst_during = safeDigitalRead(IMU_RST);
    digitalWrite(IMU_RST, HIGH);
    delay(settle_ms);
    const int int_after = safeDigitalRead(IMU_INT);
    const int rst_after = safeDigitalRead(IMU_RST);
    const int wake_after = safeDigitalRead(IMU_WAKE);
    hal_log("ESP32: IMU reset pulse low=%lums settle=%lums INT(before=%d during=%d after=%d) RST(during=%d after=%d) WAKE/PS0(before=%d after=%d)",
            (unsigned long)low_ms,
            (unsigned long)settle_ms,
            int_before, int_during, int_after,
            rst_during, rst_after,
            wake_before, wake_after);
}

bool hal_imu_read(float* yaw_rate_dps, float* roll_deg, float* heading_deg) {
    if (!yaw_rate_dps || !roll_deg || !heading_deg) return false;
    const uint32_t now_us = micros();
    const uint32_t now_ms = millis();
    if (!s_bno08x_ready || !s_reports_enabled || !s_bno08x) {
#if LOG_IMU_DIAG_INTERVAL_MS > 0
        if ((now_ms - s_last_diag_log_ms) >= LOG_IMU_DIAG_INTERVAL_MS) {
            s_last_diag_log_ms = now_ms;
            // Keep IMU diagnostics visible even if init/report setup failed.
            // This helps distinguish IMU bring-up issues from shared SPI/MISO contention.
            hal_log("IMU-DIAG: init=WAIT ready=%s reports=%s obj=%s int=%d pkt=%lu ok=%lu wait=%lu",
                    s_bno08x_ready ? "Y" : "N",
                    s_reports_enabled ? "Y" : "N",
                    s_bno08x ? "Y" : "N",
                    safeDigitalRead(IMU_INT),
                    (unsigned long)s_diag_packet_count,
                    (unsigned long)s_diag_read_ok,
                    (unsigned long)s_diag_read_wait);
        }
#endif
        s_diag_read_wait++;
        return false;
    }

    bool got_update = false;
    const bool int_asserted = pinValid(IMU_INT) ? (digitalRead(IMU_INT) == LOW) : false;
    if (int_asserted) {
        for (uint8_t i = 0; i < 4 && (pinValid(IMU_INT) ? (digitalRead(IMU_INT) == LOW) : false); i++) {
            uint8_t pkt[256] = {};
            const uint32_t request_us = hal_esp32_sensor_spi_timing_now_us();
            hal_esp32_sensor_spi_lock();
            const uint32_t lock_us = hal_esp32_sensor_spi_timing_now_us();
            const int n = s_bno08x->readPacket(pkt, sizeof(pkt));
            hal_esp32_sensor_spi_unlock();
            hal_esp32_sensor_spi_record_imu_transfer(request_us, lock_us, hal_esp32_sensor_spi_timing_now_us());
            if (n <= 0) {
                break;
            }
            s_diag_packet_count++;
            s_bno08x->processPacket(pkt, static_cast<size_t>(n));
        }
    }

    float qi = 0.0f;
    float qj = 0.0f;
    float qk = 0.0f;
    float qr = 1.0f;
    if (s_bno08x->getGameRotationVector(qi, qj, qk, qr)) {
        const uint32_t now_quat_us = micros();
        const float game_yaw = yawDegFromQuaternion(qi, qj, qk, qr);
        s_imu_roll_cache = rollDegFromQuaternion(qi, qj, qk, qr);
        if (s_game_yaw_valid && s_heading_last_us != 0 && now_quat_us > s_heading_last_us) {
            const float dt_s = static_cast<float>(now_quat_us - s_heading_last_us) / 1000000.0f;
            if (dt_s > 0.0f) {
                s_imu_yaw_cache = deltaDeg(game_yaw, s_game_yaw_cache) / dt_s;
            }
        }
        s_game_yaw_cache = game_yaw;
        s_heading_last_us = now_quat_us;
        s_game_yaw_valid = true;
        if (s_heading_ready) {
            s_imu_heading_cache =
                normalize360(s_heading_boot_mag_deg + deltaDeg(s_game_yaw_cache, s_heading_boot_game_deg));
        }
        got_update = true;
    }

    if (s_bno08x->getGeoRotationVector(qi, qj, qk, qr)) {
        s_geo_yaw_cache = yawDegFromQuaternion(qi, qj, qk, qr);
        s_geo_yaw_valid = true;
        s_geo_yaw_new = true;
        got_update = true;
    }

    updateHeadingBootstrap(now_ms);

    if (got_update) {
        s_imu_last_data_us = now_us;
        s_imu_cache_valid = true;
        s_diag_read_ok++;
    } else {
        s_diag_read_wait++;
    }

#if LOG_IMU_DIAG_INTERVAL_MS > 0
    if ((now_ms - s_last_diag_log_ms) >= LOG_IMU_DIAG_INTERVAL_MS) {
        s_last_diag_log_ms = now_ms;
        const uint32_t age_ms = s_imu_last_data_us == 0 ? 0 : ((now_us - s_imu_last_data_us) / 1000UL);
        // Keep IMU diagnostics on serial/hal_log independent from Ethernet link state
        // so bring-up and runtime troubleshooting still work offline.
        hal_log("IMU-DIAG: poll=%s int=%d data=%s age=%lums pkt=%lu game_yaw=%.1f geo_yaw=%.1f heading=%s%.1f yaw_rate=%.2f dps roll=%.2f deg boot=%u/%u ok=%lu wait=%lu",
                s_reports_enabled ? "ON" : "OFF",
                safeDigitalRead(IMU_INT),
                got_update ? "NEW" : (s_imu_cache_valid ? "CACHE" : "WAIT"),
                (unsigned long)age_ms,
                (unsigned long)s_diag_packet_count,
                s_game_yaw_cache,
                s_geo_yaw_cache,
                s_heading_ready ? "" : "WAIT:",
                s_imu_heading_cache,
                s_imu_yaw_cache,
                s_imu_roll_cache,
                (unsigned)s_heading_boot_samples,
                (unsigned)kHeadingBootMinSamples,
                (unsigned long)s_diag_read_ok,
                (unsigned long)s_diag_read_wait);
    }
#endif

    if (!s_imu_cache_valid || s_imu_last_data_us == 0 || (now_us - s_imu_last_data_us) > 500000UL) {
        return false;
    }

    *yaw_rate_dps = s_imu_yaw_cache;
    *roll_deg = s_imu_roll_cache;
    *heading_deg = s_heading_ready ? s_imu_heading_cache : 9999.0f;
    return true;
}

bool hal_imu_detect(void) {
    hal_log("ESP32: IMU detect via 7Semi BNO085 example SPI: lib=%s reports=%s",
            s_bno08x_ready ? "OK" : "FAIL",
            s_reports_enabled ? "OK" : "FAIL");
    return s_bno08x_ready;
}

bool hal_imu_probe_once(uint8_t* out_response) {
    if (!out_response) return false;
    *out_response = s_bno08x_ready ? 0x01 : 0x00;
    return s_bno08x_ready;
}

bool hal_imu_detect_boot_qualified(HalImuDetectStats* out) {
    HalImuDetectStats local = {};
    local.samples = 1;
    local.present = s_bno08x_ready;
    local.last_response = s_bno08x_ready ? 0x01 : 0x00;
    if (s_bno08x_ready) {
        local.ok_count = 1;
        local.other_count = 1;
    } else {
        local.zero_count = 1;
    }

    hal_log("ESP32: IMU boot check via 7Semi example SPI: present=%s reports=%s",
            local.present ? "YES" : "NO",
            s_reports_enabled ? "OK" : "FAIL");

    if (out) {
        *out = local;
    }
    return local.present;
}

void hal_imu_on_sensor_spi_reinit(void) {
    recoverAfterSpiReinit();
}
