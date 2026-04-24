/**
 * @file mod_wifi.h
 * @brief Transport module: WiFi AP/STA (ModuleId::WIFI).
 *
 * Stub implementation — WiFi is a fallback transport for AgSteer.
 * Starts a WiFi AP with SSID "AgSteer-Boot" on activate.
 */

#pragma once

#include "module_interface.h"

/// Module ops table for WiFi transport.
extern const ModuleOps2 mod_wifi_ops;
