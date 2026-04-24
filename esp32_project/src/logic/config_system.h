/**
 * @file config_system.h
 * @brief System-Konfigurations-Kategorie — ADR-006.
 *
 * Verwaltet systemweite Einstellungen: Log-Intervall, Aktuator-Typ.
 */

#pragma once

#include "config_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

/// System Kategorie initialisieren und registrieren.
void configSystemInit(void);

#ifdef __cplusplus
}
#endif
