/**
 * @file config_ntrip.h
 * @brief NTRIP-Konfigurations-Kategorie — ADR-006 (TASK-042).
 *
 * NTRIP Client Parameter: Host, Port, Mountpoint, User, Password.
 * Volle Implementierung folgt in Step 6.
 */

#pragma once

#include "config_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

/// NTRIP Kategorie initialisieren und registrieren.
void configNtripInit(void);

/// NTRIP Werte anzeigen.
void configNtripShow(ConfigStream output);

#ifdef __cplusplus
}
#endif
