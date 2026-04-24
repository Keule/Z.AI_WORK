/**
 * @file actuator.cpp
 * @brief Steering actuator implementation.
 *
 * Uses HAL SPI interface. Actual command protocol is stub for now.
 */

#include "actuator.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_ACT
#include "esp_log.h"
#include "log_ext.h"

namespace {
bool actuator_enabled_check() {
    return feat::act();
}
}  // namespace

void actuatorInit(void) {
    hal_actuator_begin();
    LOGI("ACT", "initialised (SPI stub)");
}

bool actuatorUpdate(uint16_t cmd) {
    hal_actuator_write(cmd);
    return true;
}

bool actuatorIsHealthy(uint32_t /*now_ms*/) {
    return true;
}
