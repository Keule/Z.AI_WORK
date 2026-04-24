/**
 * @file dependency_policy.cpp
 * @brief Runtime dependency policy implementation (freshness + plausibility).
 */

#include "dependency_policy.h"

#include <cmath>

namespace dep_policy {

bool isFresh(uint32_t now_ms, uint32_t sample_ts_ms, uint32_t timeout_ms) {
    if (sample_ts_ms == 0u) return false;
    return (now_ms - sample_ts_ms) <= timeout_ms;
}

bool isSteerAnglePlausible(float angle_deg) {
    if (!std::isfinite(angle_deg)) return false;
    return angle_deg >= -720.0f && angle_deg <= 720.0f;
}

bool isSteerAngleRawPlausible(int16_t raw_value) {
    // ADS1118 is a 16-bit ADC. Full int16_t domain is plausible.
    (void)raw_value;
    return true;
}

bool isImuPlausible(float yaw_rate_dps, float roll_deg) {
    if (!std::isfinite(yaw_rate_dps) || !std::isfinite(roll_deg)) return false;
    if (yaw_rate_dps < -2000.0f || yaw_rate_dps > 2000.0f) return false;
    if (roll_deg < -180.0f || roll_deg > 180.0f) return false;
    return true;
}

bool isHeadingPlausible(float heading_deg) {
    if (!std::isfinite(heading_deg)) return false;
    return heading_deg >= 0.0f && heading_deg < 360.0f;
}

bool isSteerAngleInputValid(uint32_t now_ms,
                            uint32_t sample_ts_ms,
                            bool quality_ok) {
    return quality_ok && isFresh(now_ms, sample_ts_ms, STEER_ANGLE_FRESHNESS_TIMEOUT_MS);
}

bool isImuInputValid(uint32_t now_ms,
                     uint32_t sample_ts_ms,
                     bool quality_ok) {
    return quality_ok && isFresh(now_ms, sample_ts_ms, IMU_FRESHNESS_TIMEOUT_MS);
}

}  // namespace dep_policy

