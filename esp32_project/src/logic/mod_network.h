/**
 * @file mod_network.h
 * @brief Protocol module: PGN Codec RX/TX over UDP (ModuleId::NETWORK).
 *
 * Wraps the existing net.h / net.cpp functionality (netPollReceive,
 * netSendAogFrames) into the ModuleOps2 interface. This is the
 * protocol layer that sits on top of transport modules (ETH, WIFI).
 */

#pragma once

#include "module_interface.h"

/// Module ops table for NETWORK protocol layer.
extern const ModuleOps2 mod_network_ops;
