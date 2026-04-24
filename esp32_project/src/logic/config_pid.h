/**
 * @file config_pid.h
 * @brief PID-Konfigurations-Kategorie — ADR-006.
 *
 * Verwaltet PID-Regler-Parameter: Kp, Ki, Kd.
 * Validierung: positive Floats mit sinnvollen Bereichen.
 *   - Kp: 0.0 – 10.0
 *   - Ki: 0.0 – 5.0
 *   - Kd: 0.0 – 1.0
 */

#pragma once

#include "config_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

/// PID Kategorie initialisieren und registrieren.
void configPidInit(void);

#ifdef __cplusplus
}
#endif
