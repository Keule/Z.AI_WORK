/**
 * @file net.h
 * @brief Network / UDP communication with AgOpenGPS / AgIO.
 *
 * Handles sending GPS and Steer status frames, and receiving
 * AgIO commands (hello, scan, subnet change, steer data).
 *
 * Uses the PGN library (pgn_types.h, pgn_codec.h, pgn_registry.h)
 * for all protocol-level encoding/decoding.
 */

#pragma once

#include "pgn_types.h"

struct NetRtcmTelemetry {
    uint32_t rx_bytes = 0;
    uint32_t dropped_packets = 0;
    uint32_t last_activity_ms = 0;
    uint32_t forwarded_bytes = 0;
    uint32_t partial_writes = 0;
    uint32_t overflow_bytes = 0;
};

/// Initialise network (W5500 Ethernet via HAL).
void netInit(void);

/// Poll for received UDP frames and process them.
/// Should be called frequently from commTask.
void netPollReceive(void);

/// Send periodic AOG frames (steer status + autosteer sensor data).
/// Should be called at ~10 Hz from commTask.
void netSendAogFrames(void);

/// Update GNSS status from UM980 parser/state before PGN 214 encoding.
/// Thread-safe via global StateLock.
void netUpdateUm980Status(uint8_t um980_fix_type,
                          bool rtcm_active,
                          uint32_t differential_age_ms);

/// Internal: process a single decoded frame.
void netProcessFrame(uint8_t src, uint8_t pgn,
                     const uint8_t* payload, size_t payload_len);

/// Snapshot RTCM receive/forward telemetry counters.
void netGetRtcmTelemetry(NetRtcmTelemetry* out);
