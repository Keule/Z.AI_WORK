/**
 * @file config_network.h
 * @brief Netzwerk-Konfigurations-Kategorie — ADR-006 (TASK-039).
 *
 * Verwaltet Netzwerk-Parameter: DHCP/Static, IP, Gateway, Subnet.
 * Validiert IP-Adressen (4 Oktette 0-255).
 */

#pragma once

#include "config_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Network Kategorie initialisieren und registrieren.
void configNetworkInit(void);

/// IP-Adresse als uint32 formatieren ( fuer Ausgabe).
void configNetworkFormatIp(uint32_t ip, char* buf, size_t buf_size);

/// IP-String parsen ( "192.168.1.70" → uint32 ).
/// @return true bei Erfolg
bool configNetworkParseIp(const char* text, uint32_t* out_ip);

#ifdef __cplusplus
}
#endif
