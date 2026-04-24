/**
 * @file hal_esp32_internal.h
 * @brief Internal HAL declarations shared between ESP32 HAL sub-modules.
 *
 * This header is NOT part of the public HAL interface (hal/hal.h).
 * It declares internal functions that are shared between the domain-specific
 * HAL implementation files (hal_spi.cpp, hal_ads1118.cpp, hal_gpio.cpp, etc.)
 * after the ADR-HAL-002 split.
 *
 * Do NOT include this header from logic/ or hal/ public code.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ===================================================================
// SPI bus internal functions (defined in hal_spi.cpp)
// Used by hal_ads1118.cpp, hal_bno085.cpp
// ===================================================================

/// Get reference to the shared sensor SPI bus object.
class SPIClass;
SPIClass& hal_esp32_sensor_spi_port(void);

/// Get current micros() for SPI timing.
uint32_t hal_esp32_sensor_spi_timing_now_us(void);

/// Acquire the SPI bus mutex (blocking).
void hal_esp32_sensor_spi_lock(void);

/// Release the SPI bus mutex.
void hal_esp32_sensor_spi_unlock(void);

/// Record IMU SPI transfer timing for telemetry.
void hal_esp32_sensor_spi_record_imu_transfer(uint32_t request_us, uint32_t lock_us, uint32_t end_us);

/// Check IMU SPI polling deadline.
void hal_esp32_imu_spi_check_deadline(uint32_t period_us, uint32_t now_us);

/// Perform a raw IMU SPI transfer via the shared bus.
bool hal_esp32_imu_raw_transfer(const uint8_t* tx, uint8_t* rx, size_t len);

/// Perform an ADS1118 SPI transfer via the shared bus (mutex-protected).
bool hal_esp32_ads_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len);

/// Read a cached ADS1118 raw sample with deadline checking.
/// Encapsulates WAS polling cache logic from the SPI telemetry domain.
int16_t hal_esp32_ads_read_raw_cached(void);

/// Actuator SPI transfer via the shared bus.
bool hal_esp32_actuator_spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len);

// ===================================================================
// Pin claim internal helpers (defined in hal_gpio.cpp)
// Used by hal_init.cpp, hal_gnss_uart.cpp
// ===================================================================

/// Reset all pin claims and set the init path name.
void hal_esp32_pin_claims_reset(const char* path);

/// Claim common init pins (SAFETY_IN, SENS_SPI pins).
bool hal_esp32_claim_common_pins(void);

/// Claim IMU and steer angle sensor pins.
bool hal_esp32_claim_imu_steer_pins(void);

/// Claim Ethernet (W5500) pins.
bool hal_esp32_claim_eth_pins(void);

/// Claim GNSS UART pins for a specific UART instance.
bool hal_esp32_claim_gnss_uart_pins(uint8_t uart_num, int rx_pin, int tx_pin);

/// Get GNSS UART default pins for a given UART number.
struct HalGnssUartPins { int8_t rx; int8_t tx; };
HalGnssUartPins hal_esp32_gnss_uart_pins_for_num(uint8_t uart_num);

#ifdef __cplusplus
}
#endif
