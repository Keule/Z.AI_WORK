/**
 * @file hal_impl.h
 * @brief ESP32-S3 HAL implementation declarations.
 */

#pragma once

#include "hal/hal.h"

/// Initialise all ESP32 HAL subsystems.
void hal_esp32_init_all(void);

/// Initialise only the subsystems needed for IMU bring-up diagnostics.
void hal_esp32_init_imu_bringup(void);

/// Initialise only communication + GNSS RTCM UART for GNSS bring-up diagnostics.
void hal_esp32_init_gnss_buildup(void);

/// Check if the shared SPI bus is currently busy (SD card access).
/// Sensor reads should be skipped when this returns true.
bool hal_spi_busy(void);


/// Select UART peripheral for GNSS RTCM stream (1 or 2).
/// Must be called before hal_gnss_rtcm_begin().
void hal_esp32_gnss_rtcm_set_uart(uint8_t uart_num);
