/**
 * @file mod_bt.h
 * @brief Transport module: Bluetooth SPP (ModuleId::BT).
 *
 * Stub implementation — provides BluetoothSerial SPP "AgSteer-Boot"
 * for diagnostic/serial access. No PGN data routed over BT currently.
 */

#pragma once

#include "module_interface.h"

/// Module ops table for Bluetooth transport.
extern const ModuleOps2 mod_bt_ops;
