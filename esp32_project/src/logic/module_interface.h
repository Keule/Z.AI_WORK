/**
 * @file module_interface.h
 * @brief Unified module system — ADR-MODULE-002.
 *
 * Defines the module ID enum, data structures (ModuleResult, ModState, ModuleOps2),
 * and the central module registry. Every module in the firmware MUST implement
 * all 15 functions defined in ModuleOps2.
 *
 * Normative reference: ADR-MODULE-002-unified-module-system.md
 */

#pragma once

#include <cstddef>
#include <cstdint>

// ===================================================================
// Module ID
// ===================================================================
enum class ModuleId : uint8_t {
    ETH      = 0,   ///< Transport: W5500 Ethernet
    WIFI,           ///< Transport: WiFi (AP/STA)
    BT,             ///< Transport: Bluetooth SPP
    NETWORK,        ///< Protocol: PGN Codec RX/TX
    GNSS,           ///< Sensor: UM980 UART (2x)
    NTRIP,          ///< Service: RTCM Client
    IMU,            ///< Sensor: BNO085 SPI
    WAS,            ///< Sensor: ADS1118 / Wheel Angle Sensor
    ACTUATOR,       ///< Actuator: DRV8263 SPI
    SAFETY,         ///< Sensor: Safety-Loop GPIO + Watchdog
    STEER,          ///< Logic: PID Controller
    LOGGING,        ///< Service: SD-Card Logger
    OTA,            ///< Service: OTA Update
    SPI,            ///< Infrastructure: sensor SPI bus (single-consumer, no mutex)
    SPI_SHARED,     ///< Infrastructure: multi-client SPI arbitration (mutex + CS)
    REMOTE_CONSOLE, ///< Service: TCP/Telnet Remote Console (DebugConsole wrapper)
    COUNT           ///< Sentinel — NOT a real module
};

/// Operating modes (ADR-MODULE-002).
enum class OpMode : uint8_t {
    CONFIG = 0,     ///< Configuration mode — pipeline stopped, CLI active
    WORK   = 1      ///< Work mode — pipeline running at 200 Hz
};

/// Convert ModuleId to human-readable C string.
const char* moduleIdToName(ModuleId id);

/// Convert ModuleId from name. Returns COUNT if not found.
ModuleId moduleIdFromName(const char* name);

// ===================================================================
// ModuleResult — returned by pipeline functions
// ===================================================================
struct ModuleResult {
    bool     success    = true;
    uint32_t error_code = 0;   ///< 0 = OK, module-specific non-zero codes
};

/// Convenience: successful No-Op result.
static constexpr ModuleResult MOD_OK = { true, 0 };

// ===================================================================
// ModState — health state per module (ADR-MODULE-001, retained)
// ===================================================================
struct ModState {
    bool     detected       = false;  ///< Hardware/dependency detected
    bool     quality_ok     = false;  ///< Data/function quality within limits
    uint32_t last_update_ms = 0;      ///< Timestamp of last successful update
    int32_t  error_code     = 0;      ///< 0 = OK, module-specific error code
};

// ===================================================================
// CfgKeyDef — descriptor for one configurable key (ADR-MODULE-002)
// ===================================================================
struct CfgKeyDef {
    const char* key;       ///< Key name for module set/get (e.g. "ip", "mode")
    const char* help;      ///< Short description (e.g. "Static IPv4 address")
};

// ===================================================================
// ModuleOps2 — unified module interface (16 function pointers)
// ===================================================================
struct ModuleOps2 {
    const char*    name;                           ///< Human-readable name
    ModuleId       id;                             ///< Module ID

    // --- Lifecycle ---
    bool           (*is_enabled)(void);            ///< Compile-time feature check
    void           (*activate)(void);              ///< Claim pins, init HW, start tasks
    void           (*deactivate)(void);            ///< Release ALL resources
    bool           (*is_healthy)(uint32_t now_ms);  ///< 4-condition health check

    // --- Pipeline (called only in WORK mode) ---
    ModuleResult   (*input)(uint32_t now_ms);      ///< Read sensors / receive data
    ModuleResult   (*process)(uint32_t now_ms);    ///< Process data
    ModuleResult   (*output)(uint32_t now_ms);     ///< Write actuators / send data

    // --- Configuration ---
    const CfgKeyDef* (*cfg_keys)(void);            ///< Return array of keys (nullptr-term, may be nullptr)
    bool           (*cfg_get)(const char* key, char* buf, size_t len);
    bool           (*cfg_set)(const char* key, const char* val);
    bool           (*cfg_apply)(void);
    bool           (*cfg_save)(void);
    bool           (*cfg_load)(void);
    bool           (*cfg_show)(void);

    // --- Diagnostics ---
    void           (*diag_info)(void);             ///< One-line health reason (via s_cli_out)
    bool           (*debug)(void);                 ///< Full verbose diagnostic report

    // --- Dependencies ---
    const ModuleId* deps;           ///< Nullptr-terminated array of required modules
};

// ===================================================================
// Per-module runtime state (managed by module_system)
// ===================================================================
struct ModuleRuntime {
    const ModuleOps2* ops;         ///< Pointer to static ops table (never null)
    bool              active;      ///< Currently activated
    ModState          state;       ///< Health state
    uint32_t          freshness_timeout_ms;  ///< Module-specific freshness timeout
};

// ===================================================================
// Central module system API
// ===================================================================

/// Initialize the module system (call once at boot).
/// Sets all modules to inactive, loads NVS config.
void moduleSysInit(void);

/// Get the module registry entry by ID.
/// Returns nullptr if id >= COUNT.
ModuleRuntime* moduleSysGet(ModuleId id);

/// Get module ops by ID (shortcut).
const ModuleOps2* moduleSysOps(ModuleId id);

/// Check if a module is currently active.
bool moduleSysIsActive(ModuleId id);

/// Activate a module (checks dependencies, claims pins).
/// Returns true on success. Logs reason on failure.
bool moduleSysActivate(ModuleId id);

/// Deactivate a module (releases all resources).
/// Returns true on success.
bool moduleSysDeactivate(ModuleId id);

/// Check module health (4-condition check via ops->is_healthy).
bool moduleSysIsHealthy(ModuleId id, uint32_t now_ms);

/// Run input phase for ALL active modules (WORK mode only).
void moduleSysRunInput(uint32_t now_ms);

/// Run process phase for ALL active modules (WORK mode only).
void moduleSysRunProcess(uint32_t now_ms);

/// Run output phase for ALL active modules (WORK mode only).
void moduleSysRunOutput(uint32_t now_ms);

/// Activate all modules that should be active at boot.
/// Called after HAL init and hardware detection.
void moduleSysBootActivate(void);

// ===================================================================
// Mode management
// ===================================================================

/// Get current operating mode.
OpMode modeGet(void);

/// Set operating mode. Returns true if transition accepted.
/// CONFIG -> WORK: checks if STEER pipeline is ready.
/// WORK -> CONFIG: always allowed.
bool modeSet(OpMode target);

/// Get mode as human-readable string.
const char* modeToString(OpMode mode);

// ===================================================================
// Legacy compatibility shims (will be removed after full migration)
// ===================================================================

/// @deprecated Use moduleSysIsActive(ModuleId::ETH) instead.
static inline bool moduleIsActiveETH(void) { return moduleSysIsActive(ModuleId::ETH); }
/// @deprecated Use moduleSysIsActive(ModuleId::IMU) instead.
static inline bool moduleIsActiveIMU(void) { return moduleSysIsActive(ModuleId::IMU); }
/// @deprecated Use moduleSysIsActive(ModuleId::WAS) instead.
static inline bool moduleIsActiveWAS(void) { return moduleSysIsActive(ModuleId::WAS); }
/// @deprecated Use moduleSysIsActive(ModuleId::ACTUATOR) instead.
static inline bool moduleIsActiveACT(void) { return moduleSysIsActive(ModuleId::ACTUATOR); }
/// @deprecated Use moduleSysIsActive(ModuleId::GNSS) instead.
static inline bool moduleIsActiveGNSS(void) { return moduleSysIsActive(ModuleId::GNSS); }
/// @deprecated Use moduleSysIsActive(ModuleId::NTRIP) instead.
static inline bool moduleIsActiveNTRIP(void) { return moduleSysIsActive(ModuleId::NTRIP); }
/// @deprecated Use moduleSysIsActive(ModuleId::SAFETY) instead.
static inline bool moduleIsActiveSAFETY(void) { return moduleSysIsActive(ModuleId::SAFETY); }
/// @deprecated Use moduleSysIsActive(ModuleId::LOGGING) instead.
static inline bool moduleIsActiveLOGGING(void) { return moduleSysIsActive(ModuleId::LOGGING); }
