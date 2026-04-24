/**
 * @file hal_gpio.cpp
 * @brief GPIO, Safety Input, SD Card Detect, and Pin Claim Arbitration.
 *
 * Domain: GPIO / Safety / Pin Claims
 * Split from hal_impl.cpp (ADR-HAL-002).
 *
 * - Safety input (active LOW on GPIO4)
 * - SD card presence detection (boot probe)
 * - Pin claim table for runtime pin arbitration (TASK-027)
 */

#include "hal/hal.h"
#include "hal_esp32_internal.h"
#include "fw_config.h"
#include "logic/log_config.h"
#include "logic/log_ext.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_HAL
#include "esp_log.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <cstring>

// ===================================================================
// Pin Claim Arbitration — TASK-027
// ===================================================================

struct PinClaimEntry {
    int pin;
    const char* owner;
};

static constexpr size_t HAL_PIN_CLAIM_CAPACITY = 32;
static PinClaimEntry s_pin_claims[HAL_PIN_CLAIM_CAPACITY] = {};
static size_t s_pin_claim_count = 0;
static const char* s_pin_claim_path = "unset";

static void pinClaimsReset(const char* path_name) {
    s_pin_claim_count = 0;
    s_pin_claim_path = path_name ? path_name : "unset";
}

static const PinClaimEntry* pinClaimFind(int pin) {
    for (size_t i = 0; i < s_pin_claim_count; ++i) {
        if (s_pin_claims[i].pin == pin) {
            return &s_pin_claims[i];
        }
    }
    return nullptr;
}

static bool pinClaimsAddBatch(const PinClaimEntry* entries, size_t count, const char* path_name) {
    if (!entries || count == 0) return true;

    for (size_t i = 0; i < count; ++i) {
        const int pin = entries[i].pin;
        if (pin < 0) continue;

        const PinClaimEntry* existing = pinClaimFind(pin);
        if (existing) {
            if (existing->owner && entries[i].owner &&
                std::strcmp(existing->owner, entries[i].owner) == 0) {
                // Idempotent re-claim by same owner is allowed.
                continue;
            }
            LOGE("HAL", "Pin claim conflict on GPIO %d (%s vs %s, init_path=%s)",
                 pin,
                 existing->owner,
                 entries[i].owner,
                 path_name ? path_name : s_pin_claim_path);
            return false;
        }

        for (size_t j = 0; j < i; ++j) {
            if (entries[j].pin == pin) {
                LOGE("HAL", "Pin claim conflict on GPIO %d within same claim batch (%s, init_path=%s)",
                     pin,
                     entries[i].owner,
                     path_name ? path_name : s_pin_claim_path);
                return false;
            }
        }
    }

    for (size_t i = 0; i < count; ++i) {
        const int pin = entries[i].pin;
        if (pin < 0) continue;
        const PinClaimEntry* existing = pinClaimFind(pin);
        if (existing && existing->owner && entries[i].owner &&
            std::strcmp(existing->owner, entries[i].owner) == 0) {
            continue;
        }
        if (s_pin_claim_count >= HAL_PIN_CLAIM_CAPACITY) {
            LOGE("HAL", "Pin claim table overflow while claiming GPIO %d (%s, init_path=%s)",
                 pin,
                 entries[i].owner,
                 path_name ? path_name : s_pin_claim_path);
            return false;
        }
        s_pin_claims[s_pin_claim_count++] = entries[i];
    }

    return true;
}

// ===================================================================
// Batch pin claim helpers (called from hal_init.cpp)
// ===================================================================

bool hal_esp32_claim_common_pins(void) {
    static constexpr PinClaimEntry claims[] = {
        {SAFETY_IN, "MOD_SAFETY"},
        {SENS_SPI_SCK, "HAL_SENSOR_SPI"},
        {SENS_SPI_MISO, "HAL_SENSOR_SPI"},
        {SENS_SPI_MOSI, "HAL_SENSOR_SPI"},
    };
    return pinClaimsAddBatch(claims, sizeof(claims) / sizeof(claims[0]), s_pin_claim_path);
}

bool hal_esp32_claim_imu_steer_pins(void) {
    static constexpr PinClaimEntry claims[] = {
        {IMU_INT, "MOD_IMU"},
        {IMU_RST, "MOD_IMU"},
        {IMU_WAKE, "MOD_IMU"},
        {CS_IMU, "MOD_IMU"},
        {CS_STEER_ANG, "MOD_ADS"},
        {CS_ACT, "MOD_ACT"},
    };
    return pinClaimsAddBatch(claims, sizeof(claims) / sizeof(claims[0]), s_pin_claim_path);
}

bool hal_esp32_claim_eth_pins(void) {
    static constexpr PinClaimEntry claims[] = {
        {ETH_SCK, "MOD_ETH"},
        {ETH_MISO, "MOD_ETH"},
        {ETH_MOSI, "MOD_ETH"},
        {ETH_CS, "MOD_ETH"},
        {ETH_INT, "MOD_ETH"},
        {ETH_RST, "MOD_ETH"},
    };
    return pinClaimsAddBatch(claims, sizeof(claims) / sizeof(claims[0]), s_pin_claim_path);
}

bool hal_esp32_claim_gnss_uart_pins(uint8_t uart_num, int rx_pin, int tx_pin) {
    auto tx_pin_is_output_capable = [](int pin) -> bool {
        if (pin < 0) return false;
#if defined(CONFIG_IDF_TARGET_ESP32)
        if (pin >= 34 && pin <= 39) return false;  // input-only on classic ESP32
#endif
        return true;
    };

    if (tx_pin < 0) {
        LOGE("HAL", "GNSS RTCM claim failed: UART%u TX unresolved", static_cast<unsigned>(uart_num));
        return false;
    }
    if (!tx_pin_is_output_capable(tx_pin)) {
        LOGE("HAL", "GNSS RTCM claim failed: UART%u TX pin %d is input-only (cannot drive UART TX)",
             static_cast<unsigned>(uart_num),
             tx_pin);
        return false;
    }
    if (rx_pin >= 38 && rx_pin <= 42) {
        LOGE("HAL", "GNSS RTCM claim failed: UART%u RX pin %d is output-only on ESP32-S3",
             static_cast<unsigned>(uart_num),
             rx_pin);
        return false;
    }

    const PinClaimEntry claims[] = {
        {rx_pin, "MOD_GNSS"},
        {tx_pin, "MOD_GNSS"},
    };
    return pinClaimsAddBatch(claims, sizeof(claims) / sizeof(claims[0]), s_pin_claim_path);
}

void hal_esp32_pin_claims_reset(const char* path) {
    pinClaimsReset(path);
}

HalGnssUartPins hal_esp32_gnss_uart_pins_for_num(uint8_t uart_num) {
    switch (uart_num) {
    case 1: return HalGnssUartPins{GNSS_UART1_RX, GNSS_UART1_TX};
    case 2: return HalGnssUartPins{GNSS_UART2_RX, GNSS_UART2_TX};
    default: return HalGnssUartPins{-1, -1};
    }
}

// ===================================================================
// Public API — Pin Claims (hal.h, extern "C")
// ===================================================================

extern "C" bool hal_pin_claim_add(int pin, const char* owner) {
    if (pin < 0 || !owner) return true;  // negative pins are harmless, skip
    if (s_pin_claim_count >= HAL_PIN_CLAIM_CAPACITY) {
        LOGE("HAL-PIN", "claim table overflow for GPIO %d (%s)", pin, owner);
        return false;
    }
    const PinClaimEntry* existing = pinClaimFind(pin);
    if (existing) {
        LOGE("HAL-PIN", "conflict on GPIO %d (%s vs %s)", pin, existing->owner, owner);
        return false;
    }
    s_pin_claims[s_pin_claim_count++] = {pin, owner};
    return true;
}

extern "C" int hal_pin_claim_release(const char* owner) {
    if (!owner) return 0;
    int released = 0;
    for (size_t i = s_pin_claim_count; i > 0; --i) {
        if (s_pin_claims[i - 1].owner != nullptr &&
            std::strcmp(s_pin_claims[i - 1].owner, owner) == 0) {
            // Remove by shifting remaining entries down
            for (size_t j = i - 1; j < s_pin_claim_count - 1; ++j) {
                s_pin_claims[j] = s_pin_claims[j + 1];
            }
            s_pin_claim_count--;
            released++;
            // Don't decrement i because we shifted entries
        }
    }
    return released;
}

extern "C" bool hal_pin_claim_check(int pin) {
    if (pin < 0) return false;
    return pinClaimFind(pin) != nullptr;
}

extern "C" const char* hal_pin_claim_owner(int pin) {
    if (pin < 0) return nullptr;
    const PinClaimEntry* entry = pinClaimFind(pin);
    if (!entry) return nullptr;
    return entry->owner;
}

// ===================================================================
// Public API — Safety and SD (hal.h)
// ===================================================================

bool hal_safety_ok(void) {
    // SAFETY_IN is active LOW: LOW = KICK, HIGH = OK
    return digitalRead(SAFETY_IN) == HIGH;
}

bool hal_sd_card_present(void) {
    static bool s_probe_done = false;
    static bool s_probe_present = false;
    if (s_probe_done) {
        return s_probe_present;
    }

    s_probe_done = true;

    // One-shot boot probe:
    // 1) release sensor SPI ownership, 2) try SD mount once, 3) always release SD SPI,
    // 4) restore sensor SPI ownership.
    hal_sensor_spi_deinit();
    hal_delay_ms(10);

    SPIClass sdSPI(SD_SPI_BUS);
    sdSPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS);

    s_probe_present = SD.begin(SD_CS, sdSPI, 4000000, "/sd", 5);
    if (s_probe_present) {
        hal_log("ESP32: SD boot probe -> PRESENT");
    } else {
        hal_log("ESP32: SD boot probe -> MISSING/INIT FAILED");
    }

    SD.end();
    sdSPI.end();
    hal_sensor_spi_reinit();
    return s_probe_present;
}
