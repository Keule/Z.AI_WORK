/**
 * @file pgn_codec.h
 * @brief AgOpenGPS PGN codec — encode, decode, validate, checksum.
 *
 * Pure C++ — no Arduino / ESP32 headers.
 * Depends only on pgn_types.h and hal/hal.h (for hal_log, hal_millis).
 *
 * Reference: https://github.com/AgOpenGPS-Official/Boards/blob/main/PGN.md
 */

#pragma once

#include "pgn_types.h"

#include <cstddef>
#include <cstdint>

// ===================================================================
// Checksum
// ===================================================================

/**
 * Compute AOG additive 8-bit checksum.
 * Sums bytes[2 .. frame_len-2] (excludes preamble [0,1] and checksum byte),
 * returns low 8 bits of the sum.
 *
 * This is NOT a CRC polynomial — it is a simple additive checksum,
 * identical to the AgOpenGPS reference implementation.
 *
 * @param frame      Complete frame including preamble (0x80 0x81 ... checksum).
 * @param frame_len  Total frame length in bytes (including preamble + checksum).
 * @return          8-bit checksum value.
 */
uint8_t pgnChecksum(const uint8_t* frame, size_t frame_len);

/**
 * Self-test: build a frame, validate round-trip, verify corruption detection.
 * Call once at startup to verify the checksum implementation.
 * @return true if all checks pass.
 */
bool pgnChecksumSelfTest(void);

// ===================================================================
// Frame Builder & Validator
// ===================================================================

/**
 * Build a complete AOG frame: 0x80 0x81 | Src | PGN | Len | payload | CRC
 *
 * @param buf          Output buffer
 * @param buf_size     Buffer size in bytes
 * @param src          Source module ID (0x7E, 0x7C, etc.)
 * @param pgn          PGN number
 * @param payload      Payload data pointer (may be nullptr for zero-length)
 * @param payload_len  Payload length in bytes
 * @return             Total bytes written to buf (header + payload + CRC),
 *                     or 0 on error (buffer too small).
 */
size_t pgnBuildFrame(uint8_t* buf, size_t buf_size,
                     uint8_t src, uint8_t pgn,
                     const void* payload, size_t payload_len);

/**
 * Validate a received AOG frame: check preamble, bounds, CRC.
 *
 * On success, fills out_src, out_pgn, out_payload, out_payload_len.
 * Rate-limits CRC mismatch logs to max 1 per 10 seconds.
 *
 * @param frame            Complete received frame
 * @param frame_len        Frame length in bytes
 * @param out_src          [out] Source module ID
 * @param out_pgn          [out] PGN number
 * @param out_payload      [out] Pointer to payload start (within frame buffer)
 * @param out_payload_len  [out] Payload length in bytes
 * @return                 true if frame is valid, false otherwise
 */
bool pgnValidateFrame(const uint8_t* frame, size_t frame_len,
                      uint8_t* out_src, uint8_t* out_pgn,
                      const uint8_t** out_payload, size_t* out_payload_len);

// ===================================================================
// Encoder functions — return total frame length including preamble & CRC
// ===================================================================

/// Encode Steer Hello Reply (PGN=0x7E, Src=0x7E, Len=5)
size_t pgnEncodeHelloReplySteer(uint8_t* buf, size_t buf_size,
                                int16_t steerAngle,
                                uint16_t sensorCounts,
                                uint8_t switchByte);

/// Encode GPS Hello Reply (PGN=0x78, Src=0x78, Len=5)
size_t pgnEncodeHelloReplyGps(uint8_t* buf, size_t buf_size);

/// Encode Subnet Reply (PGN=0xCB, Len=7)
size_t pgnEncodeSubnetReply(uint8_t* buf, size_t buf_size,
                            uint8_t src,
                            const uint8_t ip[4],
                            const uint8_t subnet[3]);

/// Encode Steer Status Out (PGN=0xFD, Src=0x7E, Len=8)
size_t pgnEncodeSteerStatusOut(uint8_t* buf, size_t buf_size,
                               int16_t actualAngleX100,
                               int16_t headingX10,
                               int16_t rollX10,
                               uint8_t switchStatus,
                               uint8_t pwmDisplay);

/// Encode From Autosteer 2 (PGN=0xFA, Src=0x7E, Len=8)
size_t pgnEncodeFromAutosteer2(uint8_t* buf, size_t buf_size,
                               uint8_t sensorValue);

/// Encode GPS Main Out (PGN=0xD6, Src=0x7C, Len=51)
size_t pgnEncodeGpsMainOut(uint8_t* buf, size_t buf_size,
                           const AogGpsMainOut& gps);

/// Encode Hardware Message (PGN=0xDD, variable length)
/// @return total frame length, or 0 on error
size_t pgnEncodeHardwareMessage(uint8_t* buf, size_t buf_size,
                                uint8_t src,
                                uint8_t duration,
                                uint8_t color,
                                const char* message);

// ===================================================================
// Decoder functions — return true if decoded successfully
// ===================================================================

/// Decode Hello From AgIO (PGN 200, Len=3)
bool pgnDecodeHelloFromAgio(const uint8_t* payload, size_t payload_len,
                            AogHelloFromAgio* out);

/// Decode Scan Request (PGN 202)
bool pgnDecodeScanRequest(const uint8_t* payload, size_t payload_len);

/// Decode Subnet Change (PGN 201, Len=5)
bool pgnDecodeSubnetChange(const uint8_t* payload, size_t payload_len,
                           AogSubnetChange* out);

/// Decode Steer Data In (PGN 254, Len=8)
bool pgnDecodeSteerDataIn(const uint8_t* payload, size_t payload_len,
                          AogSteerDataIn* out);

/// Decode Steer Settings In (PGN 252, Len=8)
bool pgnDecodeSteerSettingsIn(const uint8_t* payload, size_t payload_len,
                              AogSteerSettingsIn* out);

/// Decode Steer Config In (PGN 251, Len=8)
bool pgnDecodeSteerConfigIn(const uint8_t* payload, size_t payload_len,
                            AogSteerConfigIn* out);

/// Decode Machine Data In (PGN 239, Len=8)
bool pgnDecodeMachineDataIn(const uint8_t* payload, size_t payload_len,
                            AogMachineDataIn* out);

/// Decode Machine Config In (PGN 238, Len=3)
bool pgnDecodeMachineConfigIn(const uint8_t* payload, size_t payload_len,
                              AogMachineConfigIn* out);

/// Decode Hardware Message (PGN 221, variable length)
bool pgnDecodeHardwareMessage(const uint8_t* payload, size_t payload_len,
                              uint8_t* out_duration,
                              uint8_t* out_color,
                              char* out_message, size_t out_msg_size);

// ===================================================================
// Utility
// ===================================================================

/// Hex-dump a buffer to log (for debugging)
void pgnHexDump(const char* label, const uint8_t* data, size_t len);

/// Get human-readable PGN name for logging
const char* pgnGetName(uint8_t pgn);
