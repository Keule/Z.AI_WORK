/**
 * @file hal_spi.cpp
 * @brief Shared Sensor SPI Bus (SENS_SPI_BUS / SPI2_HOST) with telemetry.
 *
 * Domain: SPI Bus Management, IMU SPI helpers, Actuator SPI
 * Split from hal_impl.cpp (ADR-HAL-002).
 *
 * Hardware:
 *   - SPI2_HOST (SD_SPI_BUS on ESP32-S3 Arduino Core 2.x)
 *   - Pins: SCK=47, MISO=21, MOSI=38
 *   - Devices: ADS1118 ADC (CS=18), BNO085 IMU (CS=40), Actuator (CS=16)
 *
 * CRITICAL: Must use SD_SPI_BUS, NOT HSPI!
 * On ESP32-S3 (Arduino Core 2.x): HSPI = SPI3_HOST (occupied by W5500!)
 */

#include "hal/hal.h"
#include "hal_esp32_internal.h"
#include "hal_impl.h"
#include "fw_config.h"
#include "logic/log_config.h"
#include "logic/log_ext.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_HAL
#include "esp_log.h"

#include <Arduino.h>
#include <SPI.h>
#include <cstring>

// ===================================================================
// Shared sensor SPI bus instance
// ===================================================================
static SPIClass sensorSPI(SENS_SPI_BUS);

// ===================================================================
// Shared sensor SPI transaction layer (bus lock + per-device settings)
// ===================================================================
enum class SpiClient : uint8_t {
    ADS1118 = 0,
    BNO085 = 1,
    ACTUATOR = 2,
    NONE = 0xFF,
};

struct SpiClientConfig {
    int cs_pin;
    uint32_t freq_hz;
    uint8_t mode;
    uint32_t period_us;   // polling interval target
};

static const SpiClientConfig k_spi_cfg_ads = {CS_STEER_ANG, 2000000, SPI_MODE1, 10000};  // 100 Hz
static SpiClientConfig k_spi_cfg_imu = {CS_IMU,       1000000, SPI_MODE3,  5000};  // 200 Hz
static const SpiClientConfig k_spi_cfg_act = {CS_ACT,       1000000, SPI_MODE0,     0};  // event-driven

static SemaphoreHandle_t s_spi_bus_mutex = nullptr;

/// Multi-client (shared) mode flag.
/// false = DIRECT mode (1 consumer, no mutex, no other-CS deassert)
/// true  = SHARED mode (2+ consumers, full arbitration)
static bool s_multi_client = false;

// ===================================================================
// SPI Poll State and Telemetry
// ===================================================================
struct SpiPollState {
    uint32_t next_due_us = 0;
    uint32_t deadline_miss = 0;
    uint32_t transactions = 0;
    uint32_t last_us = 0;
    uint32_t max_us = 0;
};

struct SpiBusTelemetry {
    uint32_t window_start_us = 0;
    uint32_t busy_us = 0;
    uint32_t transactions = 0;
};

static SpiPollState s_poll_imu;
static SpiPollState s_poll_was;
static SpiPollState s_poll_act;
static SpiBusTelemetry s_bus_tm;
static SpiClient s_last_spi_client = SpiClient::NONE;
static uint32_t s_last_spi_end_us = 0;
static SpiClient s_last_sensor_spi_client = SpiClient::NONE;
static uint32_t s_last_sensor_spi_end_us = 0;
static uint32_t s_client_switches = 0;
static uint32_t s_was_to_imu_switches = 0;
static uint32_t s_imu_to_was_switches = 0;
static uint32_t s_other_switches = 0;
static uint32_t s_was_to_imu_gap_last_us = 0;
static uint32_t s_was_to_imu_gap_max_us = 0;
static uint32_t s_imu_to_was_gap_last_us = 0;
static uint32_t s_imu_to_was_gap_max_us = 0;
static uint32_t s_sensor_was_to_imu_switches = 0;
static uint32_t s_sensor_imu_to_was_switches = 0;
static uint32_t s_sensor_was_to_imu_gap_last_us = 0;
static uint32_t s_sensor_was_to_imu_gap_max_us = 0;
static uint32_t s_sensor_imu_to_was_gap_last_us = 0;
static uint32_t s_sensor_imu_to_was_gap_max_us = 0;

// WAS (ADS1118) polling cache — shared between SPI bus scheduling and raw reads
static int16_t s_was_raw_cache = 0;
static uint32_t s_was_last_poll_us = 0;
static bool s_was_cache_valid = false;

// ===================================================================
// Internal SPI helpers
// ===================================================================
static const SpiClientConfig& spiCfg(SpiClient client) {
    switch (client) {
    case SpiClient::ADS1118: return k_spi_cfg_ads;
    case SpiClient::BNO085: return k_spi_cfg_imu;
    case SpiClient::ACTUATOR: return k_spi_cfg_act;
    case SpiClient::NONE: break;
    }
    return k_spi_cfg_ads;
}

static void spiBeginCritical(void) {
    if (s_spi_bus_mutex) {
        xSemaphoreTake(s_spi_bus_mutex, portMAX_DELAY);
    }
}

static void spiEndCritical(void) {
    if (s_spi_bus_mutex) {
        xSemaphoreGive(s_spi_bus_mutex);
    }
}

static SpiPollState& spiPollForClient(SpiClient client) {
    switch (client) {
    case SpiClient::ADS1118: return s_poll_was;
    case SpiClient::BNO085: return s_poll_imu;
    case SpiClient::ACTUATOR: return s_poll_act;
    case SpiClient::NONE: break;
    }
    return s_poll_was;
}

static void updateLastMax(uint32_t& last_us, uint32_t& max_us, uint32_t value_us) {
    last_us = value_us;
    if (value_us > max_us) {
        max_us = value_us;
    }
}

static void spiRecordTiming(SpiClient client, uint32_t request_us, uint32_t lock_us, uint32_t end_us) {
    const uint32_t dt_us = end_us - request_us;
    SpiPollState& poll = spiPollForClient(client);
    poll.transactions++;
    poll.last_us = dt_us;
    if (dt_us > poll.max_us) {
        poll.max_us = dt_us;
    }

    s_bus_tm.busy_us += dt_us;
    s_bus_tm.transactions++;

    if (s_last_spi_client != SpiClient::NONE && s_last_spi_client != client) {
        s_client_switches++;
        const uint32_t switch_gap_us = s_last_spi_end_us == 0 ? 0 : (lock_us - s_last_spi_end_us);
        if (s_last_spi_client == SpiClient::ADS1118 && client == SpiClient::BNO085) {
            s_was_to_imu_switches++;
            updateLastMax(s_was_to_imu_gap_last_us, s_was_to_imu_gap_max_us, switch_gap_us);
        } else if (s_last_spi_client == SpiClient::BNO085 && client == SpiClient::ADS1118) {
            s_imu_to_was_switches++;
            updateLastMax(s_imu_to_was_gap_last_us, s_imu_to_was_gap_max_us, switch_gap_us);
        } else {
            s_other_switches++;
        }
    }
    s_last_spi_client = client;
    s_last_spi_end_us = end_us;

    if (client == SpiClient::ADS1118 || client == SpiClient::BNO085) {
        if (s_last_sensor_spi_client != SpiClient::NONE && s_last_sensor_spi_client != client) {
            const uint32_t sensor_gap_us = s_last_sensor_spi_end_us == 0 ? 0 : (lock_us - s_last_sensor_spi_end_us);
            if (s_last_sensor_spi_client == SpiClient::ADS1118 && client == SpiClient::BNO085) {
                s_sensor_was_to_imu_switches++;
                updateLastMax(s_sensor_was_to_imu_gap_last_us, s_sensor_was_to_imu_gap_max_us, sensor_gap_us);
            } else if (s_last_sensor_spi_client == SpiClient::BNO085 && client == SpiClient::ADS1118) {
                s_sensor_imu_to_was_switches++;
                updateLastMax(s_sensor_imu_to_was_gap_last_us, s_sensor_imu_to_was_gap_max_us, sensor_gap_us);
            }
        }
        s_last_sensor_spi_client = client;
        s_last_sensor_spi_end_us = end_us;
    }
}

static bool spiTransfer(SpiClient client, const uint8_t* tx, uint8_t* rx, size_t len) {
    const SpiClientConfig& cfg = spiCfg(client);
    if (len == 0) return true;
    if (cfg.cs_pin < 0) return false;

    const uint32_t request_us = micros();

    if (s_multi_client) {
        // ============================================================
        // SHARED MODE: full arbitration — mutex + all-CS deassert
        // + begin/endTransaction per device (different SPI settings)
        // ============================================================
        spiBeginCritical();
        const uint32_t lock_us = micros();

        // Deassert ALL CS pins to avoid ghost-driving
        if (CS_STEER_ANG >= 0) digitalWrite(CS_STEER_ANG, HIGH);
        if (CS_IMU >= 0)       digitalWrite(CS_IMU, HIGH);
        if (CS_ACT >= 0)       digitalWrite(CS_ACT, HIGH);

        sensorSPI.beginTransaction(SPISettings(cfg.freq_hz, MSBFIRST, cfg.mode));
        digitalWrite(cfg.cs_pin, LOW);
        for (size_t i = 0; i < len; i++) {
            const uint8_t v = sensorSPI.transfer(tx ? tx[i] : 0xFF);
            if (rx) rx[i] = v;
        }
        digitalWrite(cfg.cs_pin, HIGH);
        sensorSPI.endTransaction();

        spiEndCritical();
        const uint32_t end_us = micros();
        spiRecordTiming(client, request_us, lock_us, end_us);
    } else {
        // ============================================================
        // DIRECT MODE: single consumer — no mutex, no other-CS
        // deassert, just begin/endTransaction for SPI peripheral
        // settings. Minimal overhead.
        // ============================================================
        const uint32_t lock_us = request_us;  // No lock latency in direct mode

        sensorSPI.beginTransaction(SPISettings(cfg.freq_hz, MSBFIRST, cfg.mode));
        digitalWrite(cfg.cs_pin, LOW);
        for (size_t i = 0; i < len; i++) {
            const uint8_t v = sensorSPI.transfer(tx ? tx[i] : 0xFF);
            if (rx) rx[i] = v;
        }
        digitalWrite(cfg.cs_pin, HIGH);
        sensorSPI.endTransaction();

        const uint32_t end_us = micros();
        spiRecordTiming(client, request_us, lock_us, end_us);
    }

    return true;
}

static void spiCheckDeadline(SpiPollState* poll, uint32_t period_us, uint32_t now_us) {
    if (!poll || period_us == 0) return;
    if (poll->next_due_us == 0) {
        poll->next_due_us = now_us + period_us;
        return;
    }
    if (now_us > poll->next_due_us + period_us) {
        poll->deadline_miss++;
    }
    while (poll->next_due_us <= now_us) {
        poll->next_due_us += period_us;
    }
}

// ===================================================================
// Internal SPI functions — exported for BNO085 and ADS1118 sub-modules
// (declared in hal_esp32_internal.h)
// ===================================================================

void hal_esp32_imu_spi_check_deadline(uint32_t period_us, uint32_t now_us) {
    spiCheckDeadline(&s_poll_imu, period_us, now_us);
}

bool hal_esp32_imu_raw_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    return spiTransfer(SpiClient::BNO085, tx, rx, len);
}

uint32_t hal_esp32_sensor_spi_timing_now_us(void) {
    return micros();
}

void hal_esp32_sensor_spi_lock(void) {
    spiBeginCritical();
}

void hal_esp32_sensor_spi_unlock(void) {
    spiEndCritical();
}

void hal_esp32_sensor_spi_record_imu_transfer(uint32_t request_us, uint32_t lock_us, uint32_t end_us) {
    spiRecordTiming(SpiClient::BNO085, request_us, lock_us, end_us);
}

extern "C" SPIClass& hal_esp32_sensor_spi_port(void) {
    return sensorSPI;
}

// ADS1118 SPI transfer wrapper (used by hal_ads1118.cpp libdriver interface)
bool hal_esp32_ads_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    return spiTransfer(SpiClient::ADS1118, tx, rx, len);
}

// ADS1118 cached raw read — encapsulates WAS polling cache logic
// (deadline check, cache validation, SPI transfer, telemetry recording)
int16_t hal_esp32_ads_read_raw_cached(void) {
    const uint32_t now_us = micros();
    if (s_was_cache_valid && (now_us - s_was_last_poll_us) < k_spi_cfg_ads.period_us) {
        return s_was_raw_cache;
    }
    spiCheckDeadline(&s_poll_was, k_spi_cfg_ads.period_us, now_us);
    uint8_t tx[2] = {0xFF, 0xFF};
    uint8_t rx[2] = {0};
    spiTransfer(SpiClient::ADS1118, tx, rx, 2);
    s_was_raw_cache = static_cast<int16_t>((static_cast<uint16_t>(rx[0]) << 8) | rx[1]);
    s_was_last_poll_us = now_us;
    s_was_cache_valid = true;
    return s_was_raw_cache;
}

// Actuator SPI transfer wrapper
bool hal_esp32_actuator_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    return spiTransfer(SpiClient::ACTUATOR, tx, rx, len);
}

// Forward declaration — implemented in hal_bno085.cpp
extern void hal_imu_on_sensor_spi_reinit(void);

// ===================================================================
// Public API — SPI Bus (hal.h)
// ===================================================================

void hal_sensor_spi_init(void) {
    sensorSPI.begin(SENS_SPI_SCK, SENS_SPI_MISO, SENS_SPI_MOSI, -1);
    if (!s_spi_bus_mutex) {
        s_spi_bus_mutex = xSemaphoreCreateMutex();
    }
    s_bus_tm.window_start_us = micros();
    s_bus_tm.busy_us = 0;
    s_bus_tm.transactions = 0;
    s_poll_imu = {};
    s_poll_was = {};
    s_poll_act = {};
    s_last_spi_client = SpiClient::NONE;
    s_last_spi_end_us = 0;
    s_last_sensor_spi_client = SpiClient::NONE;
    s_last_sensor_spi_end_us = 0;
    s_client_switches = 0;
    s_was_to_imu_switches = 0;
    s_imu_to_was_switches = 0;
    s_other_switches = 0;
    s_was_to_imu_gap_last_us = 0;
    s_was_to_imu_gap_max_us = 0;
    s_imu_to_was_gap_last_us = 0;
    s_imu_to_was_gap_max_us = 0;
    s_sensor_was_to_imu_switches = 0;
    s_sensor_imu_to_was_switches = 0;
    s_sensor_was_to_imu_gap_last_us = 0;
    s_sensor_was_to_imu_gap_max_us = 0;
    s_sensor_imu_to_was_gap_last_us = 0;
    s_sensor_imu_to_was_gap_max_us = 0;
    s_was_cache_valid = false;
    hal_log("ESP32: sensor SPI initialised on SENS_SPI_BUS/SPI2_HOST (SCK=%d MISO=%d MOSI=%d)",
            SENS_SPI_SCK, SENS_SPI_MISO, SENS_SPI_MOSI);
}

void hal_sensor_spi_deinit(void) {
    spiBeginCritical();
    sensorSPI.end();
    spiEndCritical();
    hal_log("ESP32: shared SPI released (SENS_SPI_BUS peripheral free)");
}

void hal_sensor_spi_reinit(void) {
    spiBeginCritical();
    sensorSPI.end();
    delay(50);
    sensorSPI.begin(SENS_SPI_SCK, SENS_SPI_MISO, SENS_SPI_MOSI, -1);
    s_bus_tm.window_start_us = micros();
    s_bus_tm.busy_us = 0;
    s_bus_tm.transactions = 0;
    s_poll_imu = {};
    s_poll_was = {};
    s_poll_act = {};
    s_last_spi_client = SpiClient::NONE;
    s_last_spi_end_us = 0;
    s_last_sensor_spi_client = SpiClient::NONE;
    s_last_sensor_spi_end_us = 0;
    s_client_switches = 0;
    s_was_to_imu_switches = 0;
    s_imu_to_was_switches = 0;
    s_other_switches = 0;
    s_was_to_imu_gap_last_us = 0;
    s_was_to_imu_gap_max_us = 0;
    s_imu_to_was_gap_last_us = 0;
    s_imu_to_was_gap_max_us = 0;
    s_sensor_was_to_imu_switches = 0;
    s_sensor_imu_to_was_switches = 0;
    s_sensor_was_to_imu_gap_last_us = 0;
    s_sensor_was_to_imu_gap_max_us = 0;
    s_sensor_imu_to_was_gap_last_us = 0;
    s_sensor_imu_to_was_gap_max_us = 0;
    s_was_cache_valid = false;
    spiEndCritical();
    delay(10);
    hal_log("ESP32: shared SPI re-initialised on SD_SPI_BUS/SPI2_HOST (SCK=%d MISO=%d MOSI=%d)",
            SENS_SPI_SCK, SENS_SPI_MISO, SENS_SPI_MOSI);
    hal_imu_on_sensor_spi_reinit();
}

void hal_sensor_spi_get_telemetry(HalSpiTelemetry* out) {
    if (!out) return;
    std::memset(out, 0, sizeof(*out));
    const uint32_t now_us = micros();
    const uint32_t win_us = (s_bus_tm.window_start_us == 0 || now_us <= s_bus_tm.window_start_us)
                                ? 0
                                : (now_us - s_bus_tm.window_start_us);
    out->window_ms = win_us / 1000;
    out->bus_busy_us = s_bus_tm.busy_us;
    out->bus_transactions = s_bus_tm.transactions;
    out->imu_transactions = s_poll_imu.transactions;
    out->was_transactions = s_poll_was.transactions;
    out->actuator_transactions = s_poll_act.transactions;
    out->imu_last_us = s_poll_imu.last_us;
    out->imu_max_us = s_poll_imu.max_us;
    out->was_last_us = s_poll_was.last_us;
    out->was_max_us = s_poll_was.max_us;
    out->actuator_last_us = s_poll_act.last_us;
    out->actuator_max_us = s_poll_act.max_us;
    out->client_switches = s_client_switches;
    out->was_to_imu_switches = s_was_to_imu_switches;
    out->imu_to_was_switches = s_imu_to_was_switches;
    out->other_switches = s_other_switches;
    out->was_to_imu_gap_last_us = s_was_to_imu_gap_last_us;
    out->was_to_imu_gap_max_us = s_was_to_imu_gap_max_us;
    out->imu_to_was_gap_last_us = s_imu_to_was_gap_last_us;
    out->imu_to_was_gap_max_us = s_imu_to_was_gap_max_us;
    out->sensor_was_to_imu_switches = s_sensor_was_to_imu_switches;
    out->sensor_imu_to_was_switches = s_sensor_imu_to_was_switches;
    out->sensor_was_to_imu_gap_last_us = s_sensor_was_to_imu_gap_last_us;
    out->sensor_was_to_imu_gap_max_us = s_sensor_was_to_imu_gap_max_us;
    out->sensor_imu_to_was_gap_last_us = s_sensor_imu_to_was_gap_last_us;
    out->sensor_imu_to_was_gap_max_us = s_sensor_imu_to_was_gap_max_us;
    out->imu_deadline_miss = s_poll_imu.deadline_miss;
    out->was_deadline_miss = s_poll_was.deadline_miss;
    if (win_us > 0) {
        out->bus_utilization_pct = (100.0f * static_cast<float>(s_bus_tm.busy_us)) / static_cast<float>(win_us);
    }
}

void hal_sensor_spi_set_multi_client(bool multi_client) {
    const bool old = s_multi_client;
    s_multi_client = multi_client;
    if (old != multi_client) {
        hal_log("ESP32: SPI mode -> %s", multi_client ? "SHARED (multi-client)" : "DIRECT (single-client)");
    }
}

bool hal_sensor_spi_is_multi_client(void) {
    return s_multi_client;
}

void hal_imu_get_spi_info(HalImuSpiInfo* out) {
    if (!out) return;
    out->sck_pin = SENS_SPI_SCK;
    out->miso_pin = SENS_SPI_MISO;
    out->mosi_pin = SENS_SPI_MOSI;
    out->cs_pin = CS_IMU;
    out->int_pin = IMU_INT;
    out->freq_hz = k_spi_cfg_imu.freq_hz;
    out->mode = k_spi_cfg_imu.mode;
}

void hal_imu_set_spi_config(uint32_t freq_hz, uint8_t mode) {
    if (freq_hz == 0) return;
    k_spi_cfg_imu.freq_hz = freq_hz;
    k_spi_cfg_imu.mode = mode;
    hal_log("ESP32: IMU SPI config set (freq=%luHz mode=%u)",
            (unsigned long)k_spi_cfg_imu.freq_hz,
            (unsigned)k_spi_cfg_imu.mode);
}

// ===================================================================
// Public API — Actuator (hal.h)
// ===================================================================

void hal_actuator_begin(void) {
    if (CS_ACT < 0) {
        hal_log("ESP32: Actuator init skipped (CS_ACT not mapped on this board)");
        return;
    }
    pinMode(CS_ACT, OUTPUT);
    digitalWrite(CS_ACT, HIGH);
    hal_log("ESP32: Actuator begun on CS=%d (stub)", CS_ACT);
}

bool hal_actuator_detect(void) {
    if (CS_ACT < 0) {
        hal_log("ESP32: Actuator detect skipped (CS_ACT not mapped on this board)");
        return false;
    }
    uint8_t tx = 0x00;
    uint8_t response = 0;
    spiTransfer(SpiClient::ACTUATOR, &tx, &response, 1);
    bool detected = true;  // Stub: assume OK (actuator is write-only)
    hal_log("ESP32: Actuator detect: SPI %s", detected ? "OK" : "FAIL");
    return detected;
}

void hal_actuator_write(uint16_t cmd) {
    if (CS_ACT < 0) return;
    uint8_t tx[2] = {
        static_cast<uint8_t>((cmd >> 8) & 0xFF),
        static_cast<uint8_t>(cmd & 0xFF)
    };
    spiTransfer(SpiClient::ACTUATOR, tx, nullptr, sizeof(tx));
}
