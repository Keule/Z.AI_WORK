/**
 * @file config_menu.h
 * @brief Serial Config Menu System — ADR-006 (TASK-038).
 *
 * Blockierendes textbasiertes Menu fuer Konfigurationsaenderungen.
 * Nur aktiv im PAUSED-Modus.
 *
 * Hauptmenu: 1-6 Kategorien, S(save), L(load), F(factory), X(exit)
 * Submenu:   set <key> <value>, show, back
 * Timeout:   60 Sekunden Inaktivitaet
 */

#pragma once

#include "config_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Config Menu Status-Uebersicht anzeigen (nicht-blockierend).
void configMenuShow(ConfigStream output);

/// Config Menu starten (blockierend, mit Timeout).
/// Nur im PAUSED-Modus verfuegbar.
void configMenuStart(void);

/// Config Menu initialisieren.
void configMenuInit(void);

#ifdef __cplusplus
}
#endif
