/**
 * @file hw_status.h
 * @brief Hardware status monitoring and AgOpenGPS hardware message reporting.
 *
 * Monitors all subsystems (Ethernet, IMU, Steer Angle, Actuator, Safety)
 * and sends PGN 0xDD hardware messages to AgIO when errors are detected.
 *
 * Usage:
 *   hwStatusInit()         – call once during setup
 *   hwStatusSetFlag(id)    – set a subsystem error flag
 *   hwStatusClearFlag(id)  – clear a subsystem error flag
 *   hwStatusUpdate()       – call periodically from task_slow to check & send messages
 */

#pragma once

#include <cstddef>
#include <cstdint>

// ===================================================================
// Hardware Subsystem IDs – one bit per monitored subsystem
// ===================================================================
enum HwSubsystem : uint8_t {
    HW_ETH       = 0,   ///< W5500 Ethernet not detected / link down
    HW_IMU       = 1,   ///< BNO085 IMU communication failure
    HW_WAS       = 2,   ///< Wheel Angle Sensor (steer angle) failure
    HW_ACTUATOR  = 3,   ///< Actuator communication failure
    HW_SAFETY    = 4,   ///< Safety circuit triggered (KICK)
    HW_GNSS      = 5,   ///< GNSS subsystem (NTRIP/RTCM) failure — TASK-025
    HW_COUNT     = 6    ///< Number of monitored subsystems
};

// ===================================================================
// Hardware Status Message Severity (maps to AOG color codes)
// ===================================================================
enum HwSeverity : uint8_t {
    HW_SEV_OK      = 0,  ///< Green – subsystem working
    HW_SEV_ERROR   = 1,  ///< Red – critical failure
    HW_SEV_WARNING = 2,  ///< Yellow – degraded operation
    HW_SEV_INFO    = 3,  ///< Blue – informational
};

// ===================================================================
// Error classification for message path policy
// ===================================================================
enum HwErrorClass : uint8_t {
    HW_ERR_CLASS_STARTUP = 0,  ///< Boot-time detection/reporting path
    HW_ERR_CLASS_RUNTIME = 1   ///< Runtime monitoring/periodic path
};

enum HwErrorPriority : uint8_t {
    HW_ERR_PRIO_P1 = 0,  ///< Critical (red)
    HW_ERR_PRIO_P2 = 1,  ///< Degraded (yellow)
    HW_ERR_PRIO_P3 = 2   ///< Informational (blue/green)
};

struct HwRateLimitPolicy {
    uint32_t startup_min_interval_ms;
    uint32_t runtime_min_interval_ms;
    uint32_t runtime_error_debounce_ms;
    uint32_t runtime_resend_interval_ms;
};

// ===================================================================
// Per-subsystem status entry
// ===================================================================
struct HwSubsysStatus {
    bool     error;        ///< true = error detected
    uint8_t  severity;     ///< HwSeverity color code
    uint32_t first_seen;   ///< millis() when error was first detected
    uint32_t last_sent;    ///< millis() when last HW message was sent
};

// ===================================================================
// Initialise hardware status tracking.
// Call once during setup (after all HAL init).
// ===================================================================
void hwStatusInit(void);

// ===================================================================
// Set an error flag for a subsystem.
// ===================================================================
void hwStatusSetFlag(HwSubsystem id, HwSeverity severity = HW_SEV_ERROR);

// ===================================================================
// Clear an error flag for a subsystem.
// ===================================================================
void hwStatusClearFlag(HwSubsystem id);

// ===================================================================
// Check if a subsystem has an error.
// ===================================================================
bool hwStatusHasError(HwSubsystem id);

// ===================================================================
// Get the number of active errors.
// ===================================================================
uint8_t hwStatusErrorCount(void);

// ===================================================================
// Periodic update: check subsystems, send HW messages for active errors.
/// Should be called from task_slow at ~1 Hz (lower rate than data frames).
/// @param connected  true if Ethernet/network is up
/// @param safety_ok  true if safety circuit is OK
/// @param steer_angle_valid  true if steer angle sensor reading is plausible
/// @param imu_valid  true if IMU data is being received
/// @param ntrip_active  true if NTRIP module is active (TASK-030)
/// @return number of active errors
/// ===================================================================
uint8_t hwStatusUpdate(bool connected,
                        bool safety_ok,
                        bool steer_angle_valid,
                        bool imu_valid,
                        bool ntrip_active = true,
                        bool imu_monitored = true,
                        bool steer_monitored = true,
                        bool safety_monitored = true);

// ===================================================================
// Send a single hardware message immediately (for one-off notifications).
/// @param src      module source ID (0x7E for steer)
/// @param severity error severity / color
/// @param duration display duration in seconds (0 = persistent)
/// @param fmt      printf-style format string
/// @param ...      format arguments
// ===================================================================
void hwStatusSendMessage(uint8_t src, HwSeverity severity, uint8_t duration,
                          const char* fmt, ...);

HwSeverity hwStatusPriorityToSeverity(HwErrorPriority prio);

const HwRateLimitPolicy* hwStatusPolicy(void);

void hwStatusSendClassifiedMessage(uint8_t src,
                                   HwErrorClass err_class,
                                   HwErrorPriority prio,
                                   uint8_t duration,
                                   const char* fmt, ...);
