/**
 * @file mod_imu.h
 * @brief IMU module — BNO085 over SPI (ModuleOps2 interface).
 *
 * Migrated from imu.h/imu.cpp to the unified module system.
 * Error codes: 1=not detected, 2=read failed, 3=implausible data
 */

#pragma once

#include "module_interface.h"

/// Module ops table for the IMU module.
extern const ModuleOps2 mod_imu_ops;
