/**
 * @file was.cpp
 * @brief Wheel Angle Sensor (WAS) implementation.
 */

#include "was.h"

#include "dependency_policy.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_WAS
#include "esp_log.h"
#include "log_ext.h"

namespace {
float s_last_was_angle_deg = 0.0f;
int16_t s_last_was_raw = 0;
uint32_t s_last_was_timestamp_ms = 0;
bool s_last_was_quality = false;

bool was_enabled_check() {
    return feat::ads();
}

bool was_health_check(uint32_t now_ms) {
    return wasIsHealthy(now_ms);
}
}  // namespace

void wasInit(void) {
    hal_steer_angle_begin();
    LOGI("WAS", "initialised (SPI stub)");
}

bool wasUpdate(void) {
    s_last_was_angle_deg = hal_steer_angle_read_deg();
    s_last_was_raw = hal_steer_angle_read_raw();
    s_last_was_timestamp_ms = hal_millis();
    s_last_was_quality = dep_policy::isSteerAnglePlausible(s_last_was_angle_deg);
    return s_last_was_quality;
}

bool wasIsHealthy(uint32_t now_ms) {
    return s_last_was_quality &&
           dep_policy::isFresh(now_ms,
                               s_last_was_timestamp_ms,
                               dep_policy::STEER_ANGLE_FRESHNESS_TIMEOUT_MS);
}

float wasGetAngleDeg(void) {
    return s_last_was_angle_deg;
}

int16_t wasGetRaw(void) {
    return s_last_was_raw;
}

uint32_t wasGetTimestampMs(void) {
    return s_last_was_timestamp_ms;
}

bool wasGetQuality(void) {
    return s_last_was_quality;
}

float steerAngleReadDeg(void) {
    const bool ok = wasUpdate();
    if (!ok) return 0.0f;
    return s_last_was_angle_deg;
}
