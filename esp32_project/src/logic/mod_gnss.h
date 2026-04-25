/**
 * @file mod_gnss.h
 * @brief GNSS module — UM980 dual UART management (ModuleOps2 interface).
 *
 * New module for GNSS receiver UART initialisation and NMEA parsing.
 * Error codes: 1=uart_a_init_failed, 2=uart_b_init_failed
 */

#pragma once

#include "module_interface.h"

/// Module ops table for the GNSS module.
extern const ModuleOps2 mod_gnss_ops;
