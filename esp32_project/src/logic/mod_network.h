/**
 * @file mod_network.h
 * @brief Protocol module: PGN Codec RX/TX over UDP (ModuleId::NETWORK).
 *
 * Migrated from net.h/net.cpp. All PGN encoding/decoding for
 * communication with AgOpenGPS is now inside this module.
 * Delegates physical transport to ETH and/or WIFI modules.
 */

#pragma once

#include "module_interface.h"
#include "pgn_types.h"

/// Module ops table for the NETWORK module.
extern const ModuleOps2 mod_network_ops;

/// RTCM receive/forward telemetry counters.
struct NetRtcmTelemetry {
    uint32_t rx_bytes = 0;
    uint32_t dropped_packets = 0;
    uint32_t last_activity_ms = 0;
    uint32_t forwarded_bytes = 0;
    uint32_t partial_writes = 0;
    uint32_t overflow_bytes = 0;
};

/// Snapshot RTCM receive/forward telemetry counters.
void mod_network_get_rtcm_telemetry(NetRtcmTelemetry* out);

/// Update GNSS status from UM980 parser/state before PGN 214 encoding.
/// Thread-safe via global StateLock.
void mod_network_update_um980_status(uint8_t um980_fix_type,
                                      bool rtcm_active,
                                      uint32_t differential_age_ms);

/// Send startup error PGNs for modules that failed detection.
/// Called from main.cpp boot and internally on AgIO Hello/Scan.
void mod_network_send_startup_errors(void);
