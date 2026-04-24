/**
 * @file config_gnss.h
 * @brief GNSS-Konfigurations-Kategorie — ADR-006 (TASK-041, TASK-043).
 *
 * GNSS Parameter: Baudrate, UART-A/B Rollen und Baudraten.
 */

#pragma once

#include "config_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

/// UART-Rollen (fuer GNSS-Konfiguration).
typedef enum {
    UART_ROLE_NMEA     = 0,  ///< NMEA-Datenausgang
    UART_ROLE_RTCM     = 1,  ///< RTCM-Korrekturen Eingang
    UART_ROLE_DIAG     = 2,  ///< Diagnose-Ausgang
    UART_ROLE_DISABLED = 3   ///< Deaktiviert
} UartRole;

/// GNSS Kategorie initialisieren und registrieren.
void configGnssInit(void);

/// GNSS Werte anzeigen.
void configGnssShow(ConfigStream output);

/// Prueft ob eine Baudrate in der erlaubten Liste ist.
bool configGnssIsValidBaud(uint32_t baud);

/// UART-Rollen-Name als String.
const char* configGnssUartRoleName(uint8_t role);

#ifdef __cplusplus
}
#endif
