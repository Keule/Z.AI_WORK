/**
 * @file LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h
 * @brief Board profile for LilyGO T-ETH-Lite-S3 (ESP32-S3 + W5500).
 *
 * Target: ESP32-S3-WROOM-1-N8R8 (16 MB Flash, 8 MB Octal PSRAM) + W5500.
 *
 * SPI bus allocation (CRITICAL — wrong bus = crash):
 *   SPI3_HOST (HSPI) = W5500 Ethernet  (GPIO  9/10/11/12/13/14, fixed)
 *   SPI2_HOST (FSPI) = Sensor bus      (SCK=47, MISO=21, MOSI=38)
 *                    + SD Card (OTA)    (SCK=7,  MISO=5,  MOSI=6,  CS=42)
 *   SENS_SPI_BUS is shared — sensor SPI must be released before SD access.
 *
 * GPIO constraints (ESP32-S3R8 with Octal PSRAM):
 *   26-37: reserved by PSRAM  |  38-42: output-only  |  43-48: bidirectional
 */

#pragma once
#include <cstdint>
#define BOARD_PROFILE_NAME "lilygo_t_eth_lite_esp32_s3"
// ---------------------------------------------------------------------------
// SPI Bus 1: Ethernet - W5500 (managed by ESP-IDF ETH driver on SPI3_HOST)
// These pins are fixed by the board design - DO NOT CHANGE.
// ---------------------------------------------------------------------------



#define ETH_CS         9
#define ETH_SCK        10
#define ETH_MISO       11
#define ETH_MOSI       12
#define ETH_INT        13
#define ETH_RST        14

// ---------------------------------------------------------------------------
// SPI Bus 2: Sensor Bus (SENS_SPI_BUS = SPI2_HOST)
//
// ADS1118, IMU, and Actuator share this bus with different CS pins.
// SCK/MISO/MOSI are on GPIO 16/15/17 respectively.
// ---------------------------------------------------------------------------
#define SENS_SPI_SCK   47     // SPI clock
#define SENS_SPI_MISO  21     // SPI MISO (data from devices to ESP32)
#define SENS_SPI_MOSI  38     // SPI MOSI (data from ESP32 to devices)
#define SENS_SPI_BUS   FSPI   // SPI2_HOST on ESP32-S3 (sensor bus)

// Guard against accidental duplicate IMU pin definitions from build flags or
// other headers. This file is the single source of truth for IMU wiring.
#if defined(IMU_RST) || defined(IMU_INT) || defined(CS_IMU) || defined(IMU_WAKE)
  #error "IMU pins already defined elsewhere; keep IMU pin mapping canonical in fw_config.h"
#endif

// ---------------------------------------------------------------------------
// Current IMU Wiring (single source of truth)
// ---------------------------------------------------------------------------
#define IMU_INT        46    // BNO085 INT pin (input to ESP32-S3)
#define IMU_RST        41
#define CS_IMU          40    // BNO085 IMU
#define IMU_WAKE       15    // BNO085 PS0/WAKE (set HIGH before reset for SPI mode)

// Chip Selects (active LOW) - GPIOs 38-42 are output-only, which is fine for CS/control.

#define CS_STEER_ANG   18    // ADS1118 ADC (steer angle potentiometer)
#define CS_ACT         16    // Actuator driver






// ---------------------------------------------------------------------------
// SD Card (SD_SPI_BUS = SPI2_HOST, OTA only)
//
// The SD card uses the SAME SPI peripheral (SD_SPI_BUS) but DIFFERENT pins.
// During normal operation SD_SPI_BUS is initialised with sensor pins (15/16/17).
// For OTA firmware updates, SD_SPI_BUS is re-initialised with SD pins (5/6/7)
// via hal_sensor_spi_deinit() / hal_sensor_spi_reinit().
// ---------------------------------------------------------------------------
#define SD_SPI_SCK     7      // SPI clock for SD card
#define SD_SPI_MISO    5      // SPI MISO for SD card
#define SD_SPI_MOSI    6      // SPI MOSI for SD card
#define SD_CS          42     // SD card slot
#define SD_SPI_BUS     FSPI   // shared with sensor bus, switched at runtime

// ---------------------------------------------------------------------------
// Safety input (active LOW)
// ---------------------------------------------------------------------------
#define SAFETY_IN       4

// ---------------------------------------------------------------------------
// GNSS UART bring-up matrix (TASK-019A)
//
// Board constraints (ESP32-S3R8):
//   - GPIO 26..37: reserved by Octal PSRAM (must not be used)
//   - GPIO 38..42: output-only (must not be used as UART RX)
//
// Selected assignment:
//   - UART1 (GNSS/RTCM primary): TX=48, RX=45
//   - UART2 (GNSS/Console mirror): TX=2, RX=1
// ---------------------------------------------------------------------------
#define GNSS_UART1_TX   48
#define GNSS_UART1_RX   45
#define GNSS_UART2_TX    2
#define GNSS_UART2_RX    1

// Optional GNSS sideband lines (not wired on current board revision)
#define GNSS1_PPS_PIN   -1
#define GNSS1_EN_PIN    -1
#define GNSS2_PPS_PIN   -1
#define GNSS2_EN_PIN    -1

// GNSS console mirror defaults (diagnostic read-only sniffing in gnss_buildup)
inline constexpr uint32_t GNSS_MIRROR_BAUD = 115200;
inline constexpr int8_t GNSS_MIRROR_UART1_RX_PIN = GNSS_UART1_RX;
inline constexpr int8_t GNSS_MIRROR_UART1_TX_PIN = GNSS_UART1_TX;
inline constexpr int8_t GNSS_MIRROR_UART2_RX_PIN = GNSS_UART2_RX;
inline constexpr int8_t GNSS_MIRROR_UART2_TX_PIN = GNSS_UART2_TX;

// ---------------------------------------------------------------------------
// GNSS Receiver configuration — TASK-025
// Compile-time receiver list (board-specific).
// GNSS_RX_MAX is defined in hal.h (default: 2).
// ---------------------------------------------------------------------------

// NOTE: GNSS_RX_CONFIGS is not constexpr here because it depends on
// GNSS_RX_MAX which may differ per build environment.
// The actual configuration is applied in ntrip.cpp via ntripInit().
// Default configuration for this board (ESP32-S3):
//   inst 0: UART1 (TX48/RX45) = primary GNSS receiver
//   inst 1: UART2 (TX2/RX1)  = secondary GNSS receiver

// Logging switch (active LOW, internal pull-up)
//
// GPIO 3 — bidirectional, not used by ETH/SPI/UART/PSRAM.
// Connect a toggle switch between GPIO 3 and GND.
//   Switch OFF (open)  -> pin pulled HIGH -> logging disabled
//   Switch ON (closed) -> pin pulled LOW  -> logging enabled
// ---------------------------------------------------------------------------
#define LOG_SWITCH_PIN   3

// ---------------------------------------------------------------------------
// Feature Pin Groups — TASK-027
// Each feature lists its GPIO pins (-1 terminated).
// Used by module system for pin-claim arbitration.
//
// FirmwareFeatureId numeric mapping (matches FirmwareFeatureId enum):
//   0 = MOD_IMU, 1 = MOD_ADS, 2 = MOD_ACT, 3 = MOD_ETH,
//   4 = MOD_GNSS, 5 = MOD_NTRIP, 6 = MOD_SAFETY, 7 = MOD_LOGSW, 8 = MOD_SD
// ---------------------------------------------------------------------------

// IMU: INT=46, RST=41, WAKE=15, CS=40
static constexpr int8_t FEAT_PINS_IMU[]   = { IMU_INT, IMU_RST, IMU_WAKE, CS_IMU, -1 };
static constexpr uint8_t FEAT_PINS_IMU_COUNT = 4;

// ADS (Steer Angle Sensor): CS=18
static constexpr int8_t FEAT_PINS_ADS[]   = { CS_STEER_ANG, -1 };
static constexpr uint8_t FEAT_PINS_ADS_COUNT = 1;

// Actuator: CS=16
static constexpr int8_t FEAT_PINS_ACT[]   = { CS_ACT, -1 };
static constexpr uint8_t FEAT_PINS_ACT_COUNT = 1;

// Ethernet: SCK=10, MISO=11, MOSI=12, CS=9, INT=13, RST=14
static constexpr int8_t FEAT_PINS_ETH[]   = { ETH_SCK, ETH_MISO, ETH_MOSI, ETH_CS, ETH_INT, ETH_RST, -1 };
static constexpr uint8_t FEAT_PINS_ETH_COUNT = 6;

// GNSS: UART1 TX=48, RX=45; UART2 TX=2, RX=1
static constexpr int8_t FEAT_PINS_GNSS[]  = { GNSS_UART1_TX, GNSS_UART1_RX, GNSS_UART2_TX, GNSS_UART2_RX, -1 };
static constexpr uint8_t FEAT_PINS_GNSS_COUNT = 4;

// NTRIP: no dedicated pins (uses ETH which is already claimed)
static constexpr int8_t FEAT_PINS_NTRIP[] = { -1 };
static constexpr uint8_t FEAT_PINS_NTRIP_COUNT = 0;

// Safety: GPIO 4
static constexpr int8_t FEAT_PINS_SAFETY[] = { SAFETY_IN, -1 };
static constexpr uint8_t FEAT_PINS_SAFETY_COUNT = 1;

// Logging switch: GPIO 3
static constexpr int8_t FEAT_PINS_LOGSW[] = { LOG_SWITCH_PIN, -1 };
static constexpr uint8_t FEAT_PINS_LOGSW_COUNT = 1;

// SD subsystem: CS only (no dedicated card-detect GPIO on this board)
static constexpr int8_t FEAT_PINS_SD[] = { SD_CS, -1 };
static constexpr uint8_t FEAT_PINS_SD_COUNT = 1;

// ---------------------------------------------------------------------------
// Feature Dependencies — TASK-027
// Each feature lists the module IDs it depends on.
// Terminator is 0xFF (no valid FirmwareFeatureId can be 255).
// moduleActivate() checks that all deps are MOD_ON before activating.
// ---------------------------------------------------------------------------

// NTRIP depends on ETH being active (3 = MOD_ETH)
static constexpr uint8_t FEAT_DEPS_NTRIP[] = { 3, 0xFF };

// ACT depends on IMU and ADS (0 = MOD_IMU, 1 = MOD_ADS)
static constexpr uint8_t FEAT_DEPS_ACT[] = { 0, 1, 0xFF };
