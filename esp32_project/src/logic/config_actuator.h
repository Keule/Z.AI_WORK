/**
 * @file config_actuator.h
 * @brief Aktuator-Konfigurations-Kategorie — ADR-006.
 *
 * Verwaltet Aktuator-Typ: 0=SPI, 1=Cytron, 2=IBT2.
 */

#pragma once

#include "config_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Aktuator Kategorie initialisieren und registrieren.
void configActuatorInit(void);

#ifdef __cplusplus
}
#endif
