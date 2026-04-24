/**
 * @file mod_eth.h
 * @brief Transport module: W5500 Ethernet (ModuleId::ETH).
 *
 * Migrated from net.cpp Ethernet init. Provides the physical transport
 * layer for UDP communication with AgOpenGPS.
 */

#pragma once

#include "module_interface.h"

/// Module ops table for ETH transport.
extern const ModuleOps2 mod_eth_ops;
