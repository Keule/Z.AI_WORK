/**
 * @file hal_init.cpp
 * @brief ESP32 HAL Boot Initialization and System Timing.
 *
 * Domain: Boot / Init / Timing
 * Split from hal_impl.cpp (ADR-HAL-002).
 *
 * Contains:
 *   - hal_esp32_init_all()        — Full production boot
 *   - hal_esp32_init_imu_bringup() — IMU diagnostics-only boot
 *   - hal_esp32_init_gnss_buildup() — GNSS diagnostics-only boot
 *   - hal_millis() / hal_micros() / hal_delay_ms() — System timing
 */

#include "hal/hal.h"
#include "hal_impl.h"
#include "hal_esp32_internal.h"
#include "fw_config.h"
#include "logic/features.h"
#include "logic/log_config.h"
#include "logic/log_ext.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_HAL
#include "esp_log.h"

#include <Arduino.h>

// ===================================================================
// System Timing (hal.h)
// ===================================================================

uint32_t hal_millis(void) {
    return ::millis();
}

uint32_t hal_micros(void) {
    return ::micros();
}

void hal_delay_ms(uint32_t ms) {
    ::delay(ms);
}

// ===================================================================
// Internal: Common boot init (Serial + Mutex + Safety pin)
// ===================================================================
static void hal_esp32_common_boot_init(void) {
    // Serial
    Serial.begin(115200);
    uint32_t serial_start = millis();
    while (!Serial && (millis() - serial_start < 3000)) {
        delay(10);
    }

    // Redirect ESP-IDF log to USB CDC Serial.
    Serial.setDebugOutput(true);

    hal_log("ESP32 AgSteer starting...");

    // Mutex (creates both state mutex and log mutex)
    hal_mutex_init();

    hal_log("ESP32 seting safety pin input...");

    // Safety pin
    pinMode(SAFETY_IN, INPUT_PULLUP);
    hal_log("ESP32 safety pin set.");
}

// ===================================================================
// Public API — Boot Init Paths (hal_impl.h)
// ===================================================================

void hal_esp32_init_imu_bringup(void) {
    hal_esp32_pin_claims_reset("imu_bringup");
    if (!hal_esp32_claim_common_pins() || !hal_esp32_claim_imu_steer_pins()) {
        LOGE("HAL", "Pin claim failure in IMU bring-up path; init aborted");
        return;
    }
    hal_esp32_common_boot_init();
    // Bring-up explicitly validates shared SPI interactions.
    hal_sensor_spi_init();

    // IMU + steering-angle ADC for SPI cross-device diagnostics.
    hal_imu_begin();
    hal_imu_reset_pulse(10, 20);
    hal_steer_angle_begin();
    hal_log("ESP32: IMU bring-up HAL init complete (ADS enabled, actuator/network skipped)");
}

void hal_esp32_init_gnss_buildup(void) {
    hal_esp32_pin_claims_reset("gnss_buildup");

#if defined(GNSS_BUILDUP_RTCM_UART_NUM)
    constexpr uint8_t k_rtcm_uart_num = GNSS_BUILDUP_RTCM_UART_NUM;
#else
    constexpr uint8_t k_rtcm_uart_num = 1;
#endif

#if defined(GNSS_BUILDUP_RTCM_BAUD)
    constexpr uint32_t k_rtcm_baud = GNSS_BUILDUP_RTCM_BAUD;
#else
    constexpr uint32_t k_rtcm_baud = 115200;
#endif

#if defined(GNSS_BUILDUP_RTCM_RX_PIN)
    constexpr int8_t k_rtcm_rx_pin = static_cast<int8_t>(GNSS_BUILDUP_RTCM_RX_PIN);
#else
    constexpr int8_t k_rtcm_rx_pin = GNSS_UART1_RX;
#endif

#if defined(GNSS_BUILDUP_RTCM_TX_PIN)
    constexpr int8_t k_rtcm_tx_pin = static_cast<int8_t>(GNSS_BUILDUP_RTCM_TX_PIN);
#else
    constexpr int8_t k_rtcm_tx_pin = GNSS_UART1_TX;
#endif

    if (!hal_esp32_claim_common_pins() || !hal_esp32_claim_eth_pins()) {
        LOGE("HAL", "Pin claim failure in GNSS buildup path (common/ETH pins); init aborted");
        return;
    }

    HalGnssUartPins defaults = hal_esp32_gnss_uart_pins_for_num(k_rtcm_uart_num);
    int resolved_rx_pin = (k_rtcm_rx_pin < 0) ? static_cast<int>(defaults.rx) : static_cast<int>(k_rtcm_rx_pin);
    int resolved_tx_pin = (k_rtcm_tx_pin < 0) ? static_cast<int>(defaults.tx) : static_cast<int>(k_rtcm_tx_pin);

    bool use_default_uart_pins = false;
    if (!hal_esp32_claim_gnss_uart_pins(k_rtcm_uart_num, resolved_rx_pin, resolved_tx_pin)) {
        const bool custom_pins_requested =
            (resolved_rx_pin != static_cast<int>(defaults.rx)) ||
            (resolved_tx_pin != static_cast<int>(defaults.tx));
        if (custom_pins_requested) {
            LOGW("HAL", "GNSS RTCM pin claim conflict on custom pins (rx=%d tx=%d), fallback to UART%u defaults (rx=%d tx=%d)",
                 resolved_rx_pin,
                 resolved_tx_pin,
                 static_cast<unsigned>(k_rtcm_uart_num),
                 static_cast<int>(defaults.rx),
                 static_cast<int>(defaults.tx));
            use_default_uart_pins = true;
            resolved_rx_pin = static_cast<int>(defaults.rx);
            resolved_tx_pin = static_cast<int>(defaults.tx);
        } else {
            LOGE("HAL", "GNSS RTCM pin claim conflict on default UART%u pins; UART init disabled",
                 static_cast<unsigned>(k_rtcm_uart_num));
            return;
        }
    }

    if (use_default_uart_pins) {
        hal_esp32_pin_claims_reset("gnss_buildup");
        if (!hal_esp32_claim_common_pins() || !hal_esp32_claim_eth_pins() ||
            !hal_esp32_claim_gnss_uart_pins(k_rtcm_uart_num, resolved_rx_pin, resolved_tx_pin)) {
            LOGE("HAL", "GNSS RTCM fallback pin claim failed; UART init disabled");
            return;
        }
    }

    hal_esp32_common_boot_init();

    // Communication path required for RTCM ingress (ETH UDP).
    hal_net_init();

    // Dedicated RTCM egress over GNSS UART.
    hal_esp32_gnss_rtcm_set_uart(k_rtcm_uart_num);
    const bool gnss_uart_ok = hal_gnss_rtcm_begin(k_rtcm_baud,
                                                  static_cast<int8_t>(resolved_rx_pin),
                                                  static_cast<int8_t>(resolved_tx_pin));
    hal_log("ESP32: GNSS buildup HAL init %s (ETH=%s, UART%u baud=%lu rx=%d tx=%d)",
            gnss_uart_ok ? "complete" : "degraded",
            hal_net_is_connected() ? "UP" : "DOWN",
            static_cast<unsigned>(k_rtcm_uart_num),
            static_cast<unsigned long>(k_rtcm_baud),
            resolved_rx_pin,
            resolved_tx_pin);
}

void hal_esp32_init_all(void) {
    hal_esp32_pin_claims_reset("full_init");
    if (!hal_esp32_claim_common_pins() || !hal_esp32_claim_imu_steer_pins() || !hal_esp32_claim_eth_pins()) {
        LOGE("HAL", "Pin claim failure in full init path; init aborted");
        return;
    }
    hal_esp32_common_boot_init();
    #if FEAT_ENABLED(FEAT_COMPILED_IMU) || FEAT_ENABLED(FEAT_COMPILED_ADS) || FEAT_ENABLED(FEAT_COMPILED_ACT)
    hal_sensor_spi_init();
    #else
    hal_log("ESP32: sensor SPI init skipped (no IMU/ADS/ACT feature active)");
    #endif

    #if FEAT_ENABLED(FEAT_COMPILED_IMU)
    hal_imu_begin();
    #else
    hal_log("ESP32: IMU init skipped (FEAT_IMU=0)");
    #endif

    #if FEAT_ENABLED(FEAT_COMPILED_ADS)
    hal_steer_angle_begin();
    #else
    hal_log("ESP32: steer-angle init skipped (FEAT_ADS=0)");
    #endif

    #if FEAT_ENABLED(FEAT_COMPILED_ACT)
    hal_actuator_begin();
    #else
    hal_log("ESP32: actuator init skipped (FEAT_ACT=0)");
    #endif

    // Network (W5500 via ETH driver)
    hal_net_init();

    hal_log("ESP32: all subsystems initialised (%s)",
            hal_net_is_connected() ? "ETH UP" :
            hal_net_detected() ? "ETH no link" :
            "W5500 not found");
}
