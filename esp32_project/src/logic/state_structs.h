/**
 * @file state_structs.h
 * @brief Sub-structures for NavigationState — Phase 3.
 *
 * Each sub-struct groups fields by functional domain.
 * Rules:
 *   - Each sub-struct has exactly ONE designated writer module.
 *   - Reader modules may read from any sub-struct.
 *   - All accesses to g_nav must be protected by StateLock.
 */

#pragma once

#include <cstdint>

/**
 * OWNERSHIP RULES — ADR-007 Two-Task Architecture
 *
 * Each sub-struct has exactly ONE designated writer module.
 * Violation of these rules causes race conditions or data corruption.
 * All accesses to g_nav must be protected by StateLock (ADR-STATE-001).
 *
 * ImuState:
 *   Writer: mod_imu.cpp (mod_imu_input)
 *   Readers: mod_network.cpp (PGN 253/214), mod_logging.cpp, hw_status.cpp
 *
 * SteerState:
 *   Writer: mod_was.cpp (mod_was_input — sensor read),
 *           mod_steer.cpp (mod_steer_process — PID output)
 *   Readers: mod_network.cpp (PGN 253/250), mod_logging.cpp
 *
 * SwitchState:
 *   Writer: mod_network.cpp (mod_network_input — PGN 254)
 *           module_system.cpp (modeSet() — paused flag)
 *   Readers: mod_steer.cpp, mod_logging.cpp
 *
 * PidConfigState:
 *   Writers: mod_steer.cpp (mod_steer_process — settings_*, pid_output),
 *            mod_network.cpp (mod_network_input — PGN 251 config_*)
 *   Readers: mod_network.cpp, mod_logging.cpp
 *
 * SafetyState:
 *   Writer: mod_safety.cpp (mod_safety_input)
 *   Readers: mod_steer.cpp, mod_network.cpp, mod_logging.cpp, hw_status.cpp
 *
 * GnssState:
 *   Writer: mod_network.cpp (mod_network_input — netUpdateUm980Status())
 *   Readers: mod_network.cpp (PGN 214 encoding), main.cpp
 */

// --- IMU State (Writer: mod_imu.cpp) ---
struct ImuState {
    float    heading_deg              = 0.0f;
    float    roll_deg                 = 0.0f;
    float    yaw_rate_dps             = 0.0f;
    uint32_t heading_timestamp_ms     = 0;
    bool     heading_quality_ok       = false;
    uint32_t imu_timestamp_ms         = 0;
    bool     imu_quality_ok           = false;
};

// --- Steering State (Writer: mod_was.cpp, mod_steer.cpp) ---
struct SteerState {
    float    steer_angle_deg          = 0.0f;
    int16_t  steer_angle_raw          = 0;
    uint32_t steer_angle_timestamp_ms = 0;
    bool     steer_angle_quality_ok   = false;
};

// --- Switch / Input State (Writer: net.cpp via PGN 254; paused via module_system.cpp modeSet()) ---
struct SwitchState {
    bool     work_switch              = false;
    bool     steer_switch             = false;
    bool     paused                   = false;   // ADR-007: CONFIG-Modus (wird von modeSet() gesetzt)
    uint8_t  last_status_byte         = 0;
    float    gps_speed_kmh            = 0.0f;
    uint32_t watchdog_timer_ms        = 0;
    float    desiredSteerAngleDeg     = 0.0f;  // ADR-STATE-001: ehemals volatile, jetzt StateLock-geschuetzt
};

// --- PID / Settings State ---
struct PidConfigState {
    uint16_t pid_output               = 0;

    uint8_t  settings_kp              = 0;
    uint8_t  settings_high_pwm        = 0;
    uint8_t  settings_low_pwm         = 0;
    uint8_t  settings_min_pwm         = 0;
    uint8_t  settings_counts          = 0;
    int16_t  settings_was_offset      = 0;
    uint8_t  settings_ackerman        = 0;
    bool     settings_received        = false;

    uint8_t  config_set0              = 0;
    uint8_t  config_max_pulse         = 0;
    uint8_t  config_min_speed         = 0;
    bool     config_received          = false;
};

// --- Safety State (Writer: mod_safety.cpp) ---
struct SafetyState {
    bool     safety_ok                = false;
    bool     watchdog_triggered       = false;
};

// --- GNSS State (Writer: net.cpp via netUpdateUm980Status()) ---
struct GnssState {
    uint8_t  gps_fix_quality          = 0;
    int16_t  gps_diff_age_x100_ms     = 0;
    uint8_t  um980_fix_type           = 0;
    bool     um980_rtcm_active        = false;
    uint32_t um980_status_timestamp_ms = 0;
};
