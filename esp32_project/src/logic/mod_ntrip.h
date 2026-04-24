/**
 * @file mod_ntrip.h
 * @brief NTRIP client module — RTCM correction stream (ModuleOps2 interface).
 *
 * Migrated from ntrip.h/ntrip.cpp to the unified module system.
 * Wraps the existing NTRIP state machine and data flow functions.
 * Error codes: 1=connect_failed, 2=auth_failed, 3=disconnected
 */

#pragma once

#include "features.h"

#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)

#include "module_interface.h"

/// Module ops table for the NTRIP module.
extern const ModuleOps2 mod_ntrip_ops;

#endif // FEAT_ENABLED(FEAT_COMPILED_NTRIP)
