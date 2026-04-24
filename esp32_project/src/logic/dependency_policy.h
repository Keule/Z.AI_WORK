/**
 * @file dependency_policy.h
 * @brief Central timeout and input-quality policy for runtime dependencies.
 */

#pragma once

#include <cstdint>

namespace dep_policy {

constexpr uint32_t WATCHDOG_TIMEOUT_MS = 2500;
constexpr uint32_t STEER_ANGLE_FRESHNESS_TIMEOUT_MS = 300;
constexpr uint32_t IMU_FRESHNESS_TIMEOUT_MS = 500;

// NTRIP-specific timeouts — TASK-025
constexpr uint32_t NTRIP_RTCM_FRESHNESS_TIMEOUT_MS = 10000;  ///< No RTCM for 10s = stale
constexpr uint32_t NTRIP_RECONNECT_DELAY_MS = 5000;          ///< Default reconnect delay

bool isFresh(uint32_t now_ms, uint32_t sample_ts_ms, uint32_t timeout_ms);

bool isSteerAnglePlausible(float angle_deg);
bool isSteerAngleRawPlausible(int16_t raw_value);
bool isImuPlausible(float yaw_rate_dps, float roll_deg);
bool isHeadingPlausible(float heading_deg);

bool isSteerAngleInputValid(uint32_t now_ms,
                            uint32_t sample_ts_ms,
                            bool quality_ok);

bool isImuInputValid(uint32_t now_ms,
                     uint32_t sample_ts_ms,
                     bool quality_ok);

}  // namespace dep_policy

