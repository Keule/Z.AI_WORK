/**
 * @file module_interface.h
 * @brief Unified hardware module interface contract (Phase 2).
 *
 * Every hardware feature module SHOULD implement these four functions:
 *   xxxIsEnabled()  — compile-time feature check
 *   xxxInit()       — one-time initialisation
 *   xxxUpdate()     — periodic input/process/output cycle
 *   xxxIsHealthy()  — runtime health check (freshness timeout)
 *
 * The ModuleOps struct allows central iteration over modules.
 */

#pragma once

#include <cstdint>

/// Health check result: is the module producing fresh data?
typedef bool (*ModuleHealthFn)(uint32_t now_ms);

/// Module lifecycle operations (all optional except name).
struct ModuleOps {
    const char*     name;            // Human-readable name (e.g. "IMU")
    bool            (*isEnabled)();  // Compile-time feature check (may be nullptr)
    void            (*init)();       // One-time init (may be nullptr)
    bool            (*update)();     // Periodic update, returns success (may be nullptr)
    ModuleHealthFn  isHealthy;       // Health check with freshness timeout (may be nullptr)
};
