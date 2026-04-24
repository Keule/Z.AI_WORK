/**
 * @file imu.cpp
 * @brief IMU runtime glue for the BNO085 HAL.
 */

#include "imu.h"
#include "dependency_policy.h"
#include "global_state.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_IMU
#include "esp_log.h"
#include "log_ext.h"

void imuInit(void) {
    hal_imu_begin();
    LOGI("IMU", "initialised (BNO085 SPI)");
}

bool imuUpdate(void) {
    float yaw_rate = 0.0f;
    float roll = 0.0f;
    float heading = 9999.0f;

    if (!hal_imu_read(&yaw_rate, &roll, &heading)) {
        StateLock lock;
        g_nav.imu.imu_quality_ok = false;
        g_nav.imu.heading_quality_ok = false;
        return false;
    }

    const uint32_t now_ms = hal_millis();
    const bool plausible = dep_policy::isImuPlausible(yaw_rate, roll);
    const bool heading_plausible = dep_policy::isHeadingPlausible(heading);

    {
        StateLock lock;
        if (heading_plausible) {
            g_nav.imu.heading_deg = heading;
            g_nav.imu.heading_timestamp_ms = now_ms;
        }
        g_nav.imu.yaw_rate_dps = yaw_rate;
        g_nav.imu.roll_deg = roll;
        g_nav.imu.imu_timestamp_ms = now_ms;
        g_nav.imu.imu_quality_ok = plausible;
        g_nav.imu.heading_quality_ok = heading_plausible;
    }

    return plausible;
}

bool imuIsHealthy(uint32_t now_ms) {
    StateLock lock;
    if (!g_nav.imu.imu_quality_ok) return false;
    return dep_policy::isFresh(now_ms,
                               g_nav.imu.imu_timestamp_ms,
                               dep_policy::IMU_FRESHNESS_TIMEOUT_MS);
}

// Module ops — must be at file scope (not in anonymous namespace)
// so control.cpp can reference imu_ops
namespace {

static bool imu_module_enabled_check() {
    return feat::imu();
}

static bool imu_module_health_check(uint32_t now_ms) {
    return imuIsHealthy(now_ms);
}

}  // anonymous namespace

const ModuleOps imu_ops = {
    "IMU",
    imu_module_enabled_check,
    imuInit,
    imuUpdate,
    imu_module_health_check
};
