/**
 * @file modules.h
 * @brief AgOpenGPS module registry and management.
 *
 * Defines all AgOpenGPS network modules implemented by this firmware,
 * tracks their hardware detection status, and provides functions to:
 *   - Send hello replies for all active modules
 *   - Send subnet replies for all active modules
 *   - Report hardware errors via PGN 0xDD for failed modules
 *
 * AgOpenGPS module concept:
 *   Each "module" is a logical network entity with a unique Source ID
 *   and UDP port. AgIO discovers modules via Hello (PGN 200) and Scan
 *   (PGN 202) requests, and modules respond with hello/subnet replies.
 *
 * Modules implemented by this firmware:
 *   1. Steer   (Src=0x7E, Port=5126)
 *   2. GPS     (Src=0x7C, Port=5124)
 *   3. Machine (Src=0x7B, Port=5127)
 *
 * Reference: https://github.com/AgOpenGPS-Official/Boards/blob/main/PGN.md
 */

#pragma once

#include <cstddef>
#include <cstdint>

// ===================================================================
// AgOpenGPS Module IDs
// ===================================================================
enum AogModuleId : uint8_t {
    AOG_MOD_STEER = 0,   ///< Steering module (Src=0x7E, Port=5126)
    AOG_MOD_GPS,         ///< GPS module (Src=0x7C, Port=5124)
    AOG_MOD_MACHINE,     ///< Machine module (Src=0x7B, Port=5127)
    AOG_MOD_COUNT        ///< Number of defined modules
};

// ===================================================================
// Per-module hardware subsystem detection results
// ===================================================================
struct ModuleHwStatus {
    bool eth_detected;       ///< W5500 Ethernet chip detected
    bool imu_detected;       ///< IMU (BNO085) chip detected via SPI
    bool was_detected;       ///< Wheel Angle Sensor (steer angle) detected
    bool actuator_detected;  ///< Actuator detected via SPI
    bool safety_ok;          ///< Safety circuit reads OK
    bool sd_present;         ///< SD card presence at boot (one-shot init/mount probe)
};

// ===================================================================
// Module descriptor
// ===================================================================
struct AogModuleInfo {
    uint8_t  src_id;       ///< AgOpenGPS Source ID (0x7E, 0x78, etc.)
    uint8_t  hello_pgn;    ///< Hello reply PGN for this module
    uint16_t port;         ///< UDP port for this module's data
    const char* name;      ///< Human-readable module name
    bool     enabled;      ///< Software enabled (module is compiled in)
    bool     hw_detected;  ///< Hardware detected and operational
};

// ===================================================================
// Initialise module system.
// Runs hardware detection for all subsystems, logs results,
// and sets hw_detected for each module based on its requirements.
// Call AFTER hal_esp32_init_all() and BEFORE netInit().
// ===================================================================
void modulesInit(void);

// ===================================================================
// Get number of defined modules.
// ===================================================================
uint8_t modulesCount(void);

// ===================================================================
// Get module descriptor by index.
// Returns nullptr if index out of range.
// ===================================================================
const AogModuleInfo* modulesGet(AogModuleId id);

// ===================================================================
// Get full hardware detection results.
// ===================================================================
const ModuleHwStatus* modulesGetHwStatus(void);

// ===================================================================
// Send hello reply for ALL active modules (enabled + hardware available).
// Each module sends its specific hello PGN on its port:
//   - Steer: PGN=0x7E, Src=0x7E, Len=5 (angle+counts+switch)
// Call when AgIO Hello (PGN 200) is received.
// ===================================================================
void modulesSendHellos(void);

// ===================================================================
// Send subnet reply for ALL active modules (enabled + hardware available).
// Each module sends: PGN=0xCB, Len=7 (IP + Subnet)
// Call when AgIO Scan (PGN 202) is received.
// ===================================================================
void modulesSendSubnetReplies(void);

// ===================================================================
// Send hardware error messages (PGN 0xDD) for modules and subsystems
// where hardware was NOT detected during init.
// Call once after modulesInit() when network is up.
// Only sends messages for failures – no message if all OK.
// ===================================================================
void modulesSendStartupErrors(void);

// ===================================================================
// Check if a specific module has hardware detected.
// ===================================================================
bool modulesHwOk(AogModuleId id);

// ===================================================================
// Update dynamic hardware status (call periodically from commTask).
// Handles dynamic changes (e.g., safety circuit state changes).
// ===================================================================
void modulesUpdateStatus(void);

// ===================================================================
// Hardware Feature Module System — TASK-027
// ===================================================================
// Three-value module state: unavailable (not compiled / no pins),
// off (compiled but not active), on (compiled and active).
//
// This system is independent of the AgOpenGPS AogModuleInfo registry
// above.  AOG module hw_detected is derived from feature module states.
// ===================================================================

/// Three-value module state.
enum ModState : int8_t {
    MOD_UNAVAILABLE = -1,  ///< Not compiled in firmware, or pins not populated
    MOD_OFF         =  0,  ///< Compiled and available, but not active
    MOD_ON          =  1   ///< Compiled, available, and active
};

/// Hardware feature module IDs.
enum FirmwareFeatureId : uint8_t {
    MOD_IMU    = 0,  ///< BNO085 IMU sensor
    MOD_ADS    = 1,  ///< ADS1118 steer angle ADC
    MOD_ACT    = 2,  ///< Actuator driver
    MOD_ETH    = 3,  ///< W5500 Ethernet (or RMII on classic ESP32)
    MOD_GNSS   = 4,  ///< GNSS receivers
    MOD_NTRIP  = 5,  ///< NTRIP client
    MOD_SAFETY = 6,  ///< Safety circuit input
    MOD_LOGSW  = 7,  ///< Logging switch input
    MOD_SD     = 8,  ///< SD card subsystem (logger, OTA, runtime config)
    MOD_COUNT  = 9   ///< Total number of feature modules
};

/// Feature module descriptor.
struct FeatureModuleInfo {
    const char*    name;           ///< Human-readable name (e.g. "IMU")
    int8_t         state;          ///< Current ModState
    bool           compiled;       ///< Compiled in FW AND pins populated on this board
    bool           hw_detected;    ///< Hardware detection passed
    const int8_t*  pins;           ///< Pin array from board profile (-1 terminated)
    uint8_t        pin_count;      ///< Number of valid pins (>0)
    const uint8_t* deps;           ///< Dependency module IDs (0xFF terminated), nullptr = none
};

/// Activate a feature module at runtime.
/// Checks pin-claim conflicts and dependency modules.
/// Returns true on success.
bool moduleActivate(FirmwareFeatureId id);

/// Deactivate a feature module at runtime.
/// Releases pin claims. Returns true on success.
bool moduleDeactivate(FirmwareFeatureId id);

/// Check if a module is currently active (state == MOD_ON).
bool moduleIsActive(FirmwareFeatureId id);

/// Get the current ModState of a module.
ModState moduleGetState(FirmwareFeatureId id);

/// Get feature module descriptor by index.
/// Returns nullptr if index out of range.
const FeatureModuleInfo* moduleGetInfo(FirmwareFeatureId id);

/// Check whether the steering control pipeline is runtime-ready.
/// A ready pipeline requires active IMU + ADS + ACT modules.
/// If @p reason_buf is provided, a short diagnostic reason is written.
bool moduleControlPipelineReady(char* reason_buf = nullptr, size_t reason_buf_len = 0);
