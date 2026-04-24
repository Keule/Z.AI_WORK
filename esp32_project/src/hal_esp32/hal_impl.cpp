/**
 * @file hal_impl.cpp
 * @brief ESP32 HAL — thin dispatcher (split into domain files per ADR-HAL-002).
 *
 * This file previously contained all ESP32 HAL implementation code (~2100 lines).
 * It has been split into the following domain-specific files:
 *
 *   hal_init.cpp        — Boot initialization, system timing
 *   hal_eth.cpp         — W5500 Ethernet / network
 *   hal_spi.cpp         — Shared sensor SPI bus, IMU SPI helpers, actuator
 *   hal_ads1118.cpp     — ADS1118 ADC / steering angle sensor
 *   hal_gpio.cpp        — GPIO, safety input, SD detect, pin claim arbitration
 *   hal_gnss_uart.cpp   — GNSS UART (RTCM stream + indexed multi-receiver)
 *   hal_tcp.cpp         — TCP client (NTRIP over Ethernet)
 *   hal_logging.cpp     — USB CDC Serial logging with tag parsing
 *   hal_mutex.cpp       — FreeRTOS mutex (state + log)
 *
 * Internal cross-domain declarations: hal_esp32_internal.h
 * Public HAL interface: hal/hal.h
 * ESP32-specific declarations: hal_impl.h
 *
 * NO function implementations remain in this file.
 * All behavior is identical — this is a pure extraction.
 */
