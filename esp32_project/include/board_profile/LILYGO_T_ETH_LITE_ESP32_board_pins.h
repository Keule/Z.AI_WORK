#pragma once
#include <cstdint>
// PIN-Profil fuer LilyGO T-ETH-Lite-ESP32
#define BOARD_PROFILE_NAME "lilygo_t_eth_lite_esp32"

#define PIN_GND 100
#define PIN_5V 150
#define PIN_3V3 133
#define PIN_RST 101 
#define PIN_BT 0

#define PIN_L_1 PIN_3V3 //3V3
#define PIN_L_2 39 //IO
#define PIN_L_3 34 //SD_MISO
#define PIN_L_4 13 //SD_MOSI
#define PIN_L_5 32 //IO
#define PIN_L_6 14 //SD_SCLK
#define PIN_L_7 34 //SD_MISO
#define PIN_L_8 33 //IO
#define PIN_L_9 14 //SD_SCLK
#define PIN_L_10 13 //SD_MOSI
#define PIN_L_11 4 //IO
#define PIN_L_12 2 //IO
#define PIN_L_13 15 //IO
#define PIN_L_14 PIN_GND //GND
#define PIN_L_15 PIN_3V3 //3V3
 
 
#define PIN_R_1 PIN_5V //5V
#define PIN_R_2 PIN_GND //GND
#define PIN_R_3 1 //TXD
#define PIN_R_4 3 //RXD
#define PIN_R_5 PIN_RST //RST
#define PIN_R_6 PIN_BT //BOOT
#define PIN_R_7 5 //Out only
#define PIN_R_8 35 //IO
#define PIN_R_9 38 //Out only
#define PIN_R_10 PIN_GND //GND
#define PIN_R_11 PIN_5V //5V
#define PIN_R_12 PIN_5V //5V
#define PIN_R_13 PIN_GND //GND
#define PIN_R_14 PIN_5V //5V
#define PIN_R_15 PIN_GND //GND
 
#define ETH_TYPE                        ETH_PHY_RTL8201
#define ETH_ADDR                        0
#define ETH_CLK_MODE                    ETH_CLOCK_GPIO0_IN
#define ETH_RESET_PIN                   -1
#define ETH_MDC_PIN                     23
#define ETH_POWER_PIN                   12
#define ETH_MDIO_PIN                    18

#define SD_MISO_PIN                     34
#define SD_MOSI_PIN                     13
#define SD_SCLK_PIN                     14
#define SD_CS_PIN                       5


#define SD_SPI_SCK     SD_SCLK_PIN      // SPI clock for SD card
#define SD_SPI_MISO    SD_MISO_PIN      // SPI MISO for SD card
#define SD_SPI_MOSI    SD_MOSI_PIN      // SPI MOSI for SD card
#define SD_CS          SD_CS_PIN        // SD card slot
#define SD_SPI_BUS     HSPI             // classic ESP32 sensor/SD shared bus
 
// #define SPI_FREQUENCY  27000000
#define SPI_FREQUENCY  40000000

#define SPI_READ_FREQUENCY  20000000

#define SPI_TOUCH_FREQUENCY  2500000

// ---------------------------------------------------------------------------
// Safety input (active LOW)
// ---------------------------------------------------------------------------
#define SAFETY_IN       15



// ---------------------------------------------------------------------------
// GNSS UART bring-up matrix (TASK-019A)
//
// Selected assignment (ESP32 classic, no PSRAM constraints):
//   - UART1 (GNSS/RTCM primary): TX=2, RX=4
//   - UART2 (GNSS/Console mirror): TX=33, RX=35
// ---------------------------------------------------------------------------
#define GNSS_UART1_TX   2
#define GNSS_UART1_RX   4
#define GNSS_UART2_TX    33
#define GNSS_UART2_RX    35

// GNSS console mirror defaults (diagnostic read-only sniffing in gnss_buildup)
inline constexpr uint32_t GNSS_MIRROR_BAUD = 115200;
inline constexpr int8_t GNSS_MIRROR_UART1_RX_PIN = GNSS_UART1_RX;
inline constexpr int8_t GNSS_MIRROR_UART1_TX_PIN = GNSS_UART1_TX;
inline constexpr int8_t GNSS_MIRROR_UART2_RX_PIN = GNSS_UART2_RX;
inline constexpr int8_t GNSS_MIRROR_UART2_TX_PIN = GNSS_UART2_TX;
// ---------------------------------------------------------------------------
// Logging switch (active LOW, internal pull-up)
//
// Connect a toggle switch between LOG_SWITCH_PIN and GND.
//   Switch OFF (open)  -> pin pulled HIGH -> logging disabled
//   Switch ON (closed) -> pin pulled LOW  -> logging enabled
// ---------------------------------------------------------------------------
#define LOG_SWITCH_PIN   PIN_BT


// ---------------------------------------------------------------------------
// Optional SPI aliases used by the shared HAL code path.
//
// The classic ESP32 profile uses RMII Ethernet (ETH.begin with MDC/MDIO),
// therefore dedicated W5500 SPI pins are not used on this target.
// Keep them at -1 so pin-claim checks skip them gracefully.
// ---------------------------------------------------------------------------
#define ETH_SCK        -1
#define ETH_MISO       -1
#define ETH_MOSI       -1
#define ETH_CS         -1
#define ETH_INT        -1
#define ETH_RST        -1

// Sensor SPI bus aliases for shared HAL (legacy ESP32 wiring)
#define SENS_SPI_BUS   HSPI
#define SENS_SPI_SCK   SD_SPI_SCK
#define SENS_SPI_MISO  SD_SPI_MISO
#define SENS_SPI_MOSI  SD_SPI_MOSI


// Optional sensor/actuator control pins for feature-complete shared builds.
// Set to -1 on this board profile if a line is not populated.
#define IMU_INT        -1
#define IMU_RST        -1
#define IMU_WAKE       -1
#define CS_IMU         -1
#define CS_STEER_ANG   -1
#define CS_ACT         -1

// ---------------------------------------------------------------------------
// Feature Pin Groups — TASK-027
// ESP32 Classic board — many sensor pins are not populated (-1).
// Used by module system for pin-claim arbitration.
//
// FirmwareFeatureId numeric mapping (matches FirmwareFeatureId enum):
//   0 = MOD_IMU, 1 = MOD_ADS, 2 = MOD_ACT, 3 = MOD_ETH,
//   4 = MOD_GNSS, 5 = MOD_NTRIP, 6 = MOD_SAFETY, 7 = MOD_LOGSW, 8 = MOD_SD
// ---------------------------------------------------------------------------

static constexpr int8_t FEAT_PINS_IMU[]   = { -1 };  // not populated on this board
static constexpr uint8_t FEAT_PINS_IMU_COUNT = 0;

static constexpr int8_t FEAT_PINS_ADS[]   = { -1 };  // not populated on this board
static constexpr uint8_t FEAT_PINS_ADS_COUNT = 0;

static constexpr int8_t FEAT_PINS_ACT[]   = { -1 };  // not populated on this board
static constexpr uint8_t FEAT_PINS_ACT_COUNT = 0;

// ESP32 Classic uses RMII Ethernet (not SPI), only MDC/MDIO pins
static constexpr int8_t FEAT_PINS_ETH[]   = { ETH_MDC_PIN, ETH_MDIO_PIN, -1 };
static constexpr uint8_t FEAT_PINS_ETH_COUNT = 2;

static constexpr int8_t FEAT_PINS_GNSS[]  = { GNSS_UART1_TX, GNSS_UART1_RX, GNSS_UART2_TX, GNSS_UART2_RX, -1 };
static constexpr uint8_t FEAT_PINS_GNSS_COUNT = 4;

static constexpr int8_t FEAT_PINS_NTRIP[] = { -1 };
static constexpr uint8_t FEAT_PINS_NTRIP_COUNT = 0;

static constexpr int8_t FEAT_PINS_SAFETY[] = { SAFETY_IN, -1 };
static constexpr uint8_t FEAT_PINS_SAFETY_COUNT = 1;

// LOG_SWITCH_PIN = 5 on this board
static constexpr int8_t FEAT_PINS_LOGSW[] = { LOG_SWITCH_PIN, -1 };
static constexpr uint8_t FEAT_PINS_LOGSW_COUNT = 1;

// SD subsystem: CS only (no dedicated card-detect GPIO on this board)
static constexpr int8_t FEAT_PINS_SD[] = { SD_CS, -1 };
static constexpr uint8_t FEAT_PINS_SD_COUNT = 1;

// ---------------------------------------------------------------------------
// Feature Dependencies — TASK-027
// Each feature lists the module IDs it depends on.
// Terminator is 0xFF (no valid FirmwareFeatureId can be 255).
// ---------------------------------------------------------------------------

// NTRIP depends on ETH being active (3 = MOD_ETH)
static constexpr uint8_t FEAT_DEPS_NTRIP[] = { 3, 0xFF };

// ACT depends on IMU and ADS (0 = MOD_IMU, 1 = MOD_ADS)
static constexpr uint8_t FEAT_DEPS_ACT[] = { 0, 1, 0xFF };
