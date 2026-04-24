/**
 * @file hal.h
 * @brief Hardware Abstraction Layer – abstract interface for all HW access.
 *
 * This header contains ONLY declarations with C linkage.
 * Implementations live in hal_esp32/ or hal_pc/.
 *
 * No Arduino or ESP32 headers may be included here.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ===================================================================
// System / Timing
// ===================================================================

/// Return milliseconds since boot (like Arduino millis()).
uint32_t hal_millis(void);

/// Return microseconds since boot.
uint32_t hal_micros(void);

/// Blocking delay in milliseconds.
void hal_delay_ms(uint32_t ms);

// ===================================================================
// Logging
// ===================================================================

/// Print a log message (printf-style on PC, Serial on ESP32).
void hal_log(const char* fmt, ...);

// ===================================================================
// Mutex (used by global_state.h)
// ===================================================================

void hal_mutex_init(void);
void hal_mutex_lock(void);
void hal_mutex_unlock(void);

// ===================================================================
// Safety input
// ===================================================================

/// Read safety input – true if OK (HIGH), false if KICK (LOW).
bool hal_safety_ok(void);

// ===================================================================
// SD card detect
// ===================================================================

/// Read SD card presence at boot.
/// Performs a one-shot SD init/mount probe and restores the SPI bus afterwards.
bool hal_sd_card_present(void);

// ===================================================================
// SPI Sensors / Actuator
// ===================================================================

/// Aggregated telemetry for the shared sensor SPI bus.
typedef struct {
    uint32_t window_ms;
    float bus_utilization_pct;
    uint32_t bus_transactions;
    uint32_t bus_busy_us;
    uint32_t imu_transactions;
    uint32_t was_transactions;
    uint32_t actuator_transactions;
    uint32_t imu_last_us;
    uint32_t imu_max_us;
    uint32_t was_last_us;
    uint32_t was_max_us;
    uint32_t actuator_last_us;
    uint32_t actuator_max_us;
    uint32_t client_switches;
    uint32_t was_to_imu_switches;
    uint32_t imu_to_was_switches;
    uint32_t other_switches;
    uint32_t was_to_imu_gap_last_us;
    uint32_t was_to_imu_gap_max_us;
    uint32_t imu_to_was_gap_last_us;
    uint32_t imu_to_was_gap_max_us;
    uint32_t sensor_was_to_imu_switches;
    uint32_t sensor_imu_to_was_switches;
    uint32_t sensor_was_to_imu_gap_last_us;
    uint32_t sensor_was_to_imu_gap_max_us;
    uint32_t sensor_imu_to_was_gap_last_us;
    uint32_t sensor_imu_to_was_gap_max_us;
    uint32_t imu_deadline_miss;
    uint32_t was_deadline_miss;
} HalSpiTelemetry;

/// Static IMU SPI wiring/configuration info for diagnostics/bring-up logs.
typedef struct {
    int sck_pin;
    int miso_pin;
    int mosi_pin;
    int cs_pin;
    int int_pin;
    uint32_t freq_hz;
    uint8_t mode;
} HalImuSpiInfo;

/// Boot-time IMU detection statistics (multi-sample qualification).
typedef struct {
    uint16_t samples;
    uint16_t ok_count;
    uint16_t ff_count;
    uint16_t zero_count;
    uint16_t other_count;
    uint8_t last_response;
    bool present;
} HalImuDetectStats;

/// Initialise SPI bus 2 (sensor bus) and all chip selects.
void hal_sensor_spi_init(void);

/// Release SPI bus 2 (sensor bus).
/// Must be called before another peripheral (e.g. SD card) can use SPI2_HOST.
void hal_sensor_spi_deinit(void);

/// Re-initialise SPI bus 2 (sensor bus) after a temporary release.
/// Used to restore the sensor bus after an SD card operation.
void hal_sensor_spi_reinit(void);

/// Copy current SPI telemetry counters into @p out.
void hal_sensor_spi_get_telemetry(HalSpiTelemetry* out);

// --- IMU (BNO085) ---

/// Initialise IMU on SPI.
void hal_imu_begin(void);

/// Read yaw rate, roll, and heading from IMU. Returns true on success.
bool hal_imu_read(float* yaw_rate_dps, float* roll_deg, float* heading_deg);

/// Detect if IMU chip is present on SPI bus (call after hal_imu_begin).
/// Performs a chip ID read to verify hardware responds.
bool hal_imu_detect(void);

/// Pulse IMU reset line (active LOW) for bring-up diagnostics.
void hal_imu_reset_pulse(uint32_t low_ms, uint32_t settle_ms);

/// Boot-time qualified IMU detection.
/// Performs multiple samples and classifies responses.
bool hal_imu_detect_boot_qualified(HalImuDetectStats* out);

/// Return IMU SPI pin mapping + SPI settings used by HAL.
void hal_imu_get_spi_info(HalImuSpiInfo* out);

/// Override IMU SPI bus parameters at runtime (bring-up diagnostics).
void hal_imu_set_spi_config(uint32_t freq_hz, uint8_t mode);

/// Perform one raw IMU SPI probe transfer and return response byte.
bool hal_imu_probe_once(uint8_t* out_response);

// --- Steering Angle Sensor ---

/// Initialise steering angle sensor on SPI.
void hal_steer_angle_begin(void);

/// Detect if steer angle sensor hardware is present (call after hal_steer_angle_begin).
bool hal_steer_angle_detect(void);

/// Run interactive steering angle calibration.
/// Prompts via Serial: move to left stop, then right stop.
/// Stores calibrated min/max raw ADC values in NVS (persistent).
/// Must be called AFTER hal_steer_angle_detect() succeeds.
void hal_steer_angle_calibrate(void);

/// Check if steering angle has a valid calibration stored.
/// Returns true if calibration was loaded from NVS.
bool hal_steer_angle_is_calibrated(void);

/// Read current steering angle in degrees.
/// Uses calibrated min/max to map ADC to -22.5°..+22.5°.
/// If not calibrated, returns 0.0.
float hal_steer_angle_read_deg(void);

/// Read raw 16-bit ADC value from ADS1118 (single sample, no median).
/// Returns 0 if not detected or not calibrated.
int16_t hal_steer_angle_read_raw(void);

/// Read the PGN 250 compatible 8-bit steer sensor value.
/// Calibrated sensors are scaled to 0..255; uncalibrated sensors use raw low byte.
uint8_t hal_steer_angle_read_sensor_byte(void);

// --- Actuator ---

/// Initialise actuator SPI interface.
void hal_actuator_begin(void);

/// Write a command value (0..65535) to the actuator.
void hal_actuator_write(uint16_t cmd);

/// Detect if actuator hardware is present (call after hal_actuator_begin).
bool hal_actuator_detect(void);


// ===================================================================
// GNSS RTCM (UART)
// ===================================================================

/// Initialise GNSS RTCM UART output (UM980: 8N1).
/// @param baud    UART baud rate (e.g. 115200).
/// @param rx_pin  UART RX GPIO, or -1 to keep unbound.
/// @param tx_pin  UART TX GPIO.
/// @return true on success.
bool hal_gnss_rtcm_begin(uint32_t baud, int8_t rx_pin, int8_t tx_pin);

/// Write RTCM bytes to the GNSS UART stream.
/// @return number of bytes accepted by UART driver.
size_t hal_gnss_rtcm_write(const uint8_t* data, size_t len);

/// True after successful hal_gnss_rtcm_begin().
bool hal_gnss_rtcm_is_ready(void);

/// Total number of bytes dropped due to partial/failed UART writes.
uint32_t hal_gnss_rtcm_drop_count(void);

// ===================================================================
// GNSS UART (indexed, multi-receiver support) — TASK-025
// ===================================================================
/// Maximum number of GNSS receivers (compile-time, board-specific).
#ifndef GNSS_RX_MAX
#define GNSS_RX_MAX 2
#endif

/// Initialise GNSS UART for a specific receiver instance.
/// @param inst    receiver index (0..GNSS_RX_MAX-1)
/// @param baud    UART baud rate (e.g. 115200).
/// @param rx_pin  UART RX GPIO, or -1 for default.
/// @param tx_pin  UART TX GPIO.
/// @return true on success.
bool hal_gnss_uart_begin(uint8_t inst, uint32_t baud, int8_t rx_pin, int8_t tx_pin);

/// Write RTCM data to a specific GNSS receiver instance.
/// @return number of bytes accepted by UART driver.
size_t hal_gnss_uart_write(uint8_t inst, const uint8_t* data, size_t len);

/// Check if a GNSS receiver instance is initialised and ready.
bool hal_gnss_uart_is_ready(uint8_t inst);

// ===================================================================
// TCP Client (NTRIP over Ethernet) — TASK-025
// ===================================================================

/// Open a TCP connection to host:port over Ethernet.
/// @return true on success.
bool hal_tcp_connect(const char* host, uint16_t port);

/// Send data over the TCP connection.
/// @return number of bytes sent, or 0 on error.
size_t hal_tcp_write(const uint8_t* data, size_t len);

/// Read available data from the TCP connection (non-blocking).
/// @return number of bytes read, or 0 if nothing available.
int hal_tcp_read(uint8_t* buf, size_t max_len);

/// Check how many bytes are available to read from the TCP connection.
/// @return number of available bytes, or -1 if not connected.
int hal_tcp_available(void);

/// Check if TCP connection is currently active.
bool hal_tcp_connected(void);

/// Close the TCP connection.
void hal_tcp_disconnect(void);

// ===================================================================
// Network (W5500 Ethernet)
// ===================================================================

/// Initialise W5500 Ethernet and configure IP.
void hal_net_init(void);

/// Update the destination IP address for outgoing UDP datagrams.
/// Call this after receiving a Subnet Change (PGN 201) from AgIO.
void hal_net_set_dest_ip(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);

/// Send a UDP datagram to the configured AgIO endpoint.
/// @param data     payload bytes (caller must include AOG header + data + CRC)
/// @param len      total length of the datagram
/// @param port     destination UDP port (host byte order)
void hal_net_send(const uint8_t* data, size_t len, uint16_t port);

/// Poll for a received UDP datagram (non-blocking).
/// @param buf      buffer to receive into
/// @param max_len  buffer size
/// @param out_port receives the source UDP port (host byte order)
/// @return number of bytes received, or 0 if nothing available
int hal_net_receive(uint8_t* buf, size_t max_len, uint16_t* out_port);

/// Poll for a received RTCM UDP datagram from the dedicated RTCM socket.
/// @param buf      buffer to receive into
/// @param max_len  buffer size
/// @param out_port receives the source UDP port (host byte order)
/// @return number of bytes received, or 0 if nothing available
int hal_net_receive_rtcm(uint8_t* buf, size_t max_len, uint16_t* out_port);

/// Forward RTCM correction bytes to the GNSS receiver transport.
/// Implementations may accept partial writes for non-blocking behavior.
/// @return number of bytes accepted for forwarding
size_t hal_gnss_rtcm_write(const uint8_t* data, size_t len);

/// Check if Ethernet link is up.
bool hal_net_is_connected(void);

/// Check if W5500 chip was detected during init.
bool hal_net_detected(void);

/// Set static network parameters used for next restart.
void hal_net_set_static_config(uint32_t ip, uint32_t gw, uint32_t subnet);

/// Restart Ethernet stack (blocking).
bool hal_net_restart(void);

/// Current IPv4 values (big-endian u32: a.b.c.d => 0xAABBCCDD).
uint32_t hal_net_get_ip(void);
uint32_t hal_net_get_gateway(void);
uint32_t hal_net_get_subnet(void);

/// Ethernet link state (PHY/link only).
bool hal_net_link_up(void);

// ===================================================================
// Pin Claim Arbitration — TASK-027
// ===================================================================

/// Add a pin claim for a given owner string.
/// @param pin   GPIO pin number (negative pins are rejected silently).
/// @param owner Null-terminated owner identifier (e.g. "MOD_IMU").
/// @return true on success, false if pin is already claimed by another owner
///         or if the claim table is full.
bool hal_pin_claim_add(int pin, const char* owner);

/// Release all pin claims owned by the given owner string.
/// @param owner Null-terminated owner identifier.
/// @return Number of pins released, or 0 if owner not found / invalid.
int hal_pin_claim_release(const char* owner);

/// Check if a specific GPIO pin is currently claimed.
/// @param pin GPIO pin number.
/// @return true if claimed, false if not or pin < 0.
bool hal_pin_claim_check(int pin);

/// Get the owner string of a claimed GPIO pin.
/// @param pin GPIO pin number.
/// @return Owner identifier string, or nullptr if pin is not claimed or pin < 0.
const char* hal_pin_claim_owner(int pin);

#ifdef __cplusplus
}
#endif
