/**
 * @file aog_udp_protocol.h
 * @brief Backward-compatibility wrapper — DEPRECATED.
 *
 * This header provides backward-compatible aliases for all old
 * constants and functions that have been moved to the PGN library:
 *   - pgn_types.h   (constants, structs)
 *   - pgn_codec.h   (encode/decode/checksum functions)
 *   - pgn_registry.h (PGN directory)
 *
 * New code should include pgn_types.h, pgn_codec.h, pgn_registry.h directly.
 * This file will be removed in a future version.
 */

#pragma once

// Redirect to the new PGN library headers
#include "pgn_types.h"
#include "pgn_codec.h"
#include "pgn_registry.h"

// ===================================================================
// Backward-compatible constant aliases
// ===================================================================

// Preamble
constexpr uint8_t AOG_ID_1 = AOG_PREAMBLE_1;
constexpr uint8_t AOG_ID_2 = AOG_PREAMBLE_2;

// Source IDs
constexpr uint8_t AOG_SRC_AGIO       = aog_src::AGIO;
constexpr uint8_t AOG_SRC_STEER      = aog_src::STEER;
constexpr uint8_t AOG_SRC_MACHINE    = aog_src::MACHINE;
constexpr uint8_t AOG_SRC_GPS        = aog_src::GPS;
constexpr uint8_t AOG_SRC_GPS_REPLY  = aog_src::GPS_REPLY;

// PGN Numbers
constexpr uint8_t PGN_HELLO_FROM_AGIO   = aog_pgn::HELLO_FROM_AGIO;
constexpr uint8_t PGN_SUBNET_CHANGE     = aog_pgn::SUBNET_CHANGE;
constexpr uint8_t PGN_SCAN_REQUEST      = aog_pgn::SCAN_REQUEST;
constexpr uint8_t PGN_SUBNET_REPLY      = aog_pgn::SUBNET_REPLY;
constexpr uint8_t PGN_HELLO_REPLY_STEER = aog_pgn::HELLO_REPLY_STEER;
constexpr uint8_t PGN_HELLO_REPLY_GPS   = aog_pgn::HELLO_REPLY_GPS;
constexpr uint8_t PGN_STEER_DATA_IN     = aog_pgn::STEER_DATA_IN;
constexpr uint8_t PGN_STEER_STATUS_OUT  = aog_pgn::STEER_STATUS_OUT;
constexpr uint8_t PGN_STEER_SETTINGS_IN = aog_pgn::STEER_SETTINGS_IN;
constexpr uint8_t PGN_STEER_CONFIG_IN   = aog_pgn::STEER_CONFIG_IN;
constexpr uint8_t PGN_FROM_AUTOSTEER_2  = aog_pgn::FROM_AUTOSTEER_2;
constexpr uint8_t PGN_GPS_MAIN_OUT      = aog_pgn::GPS_MAIN_OUT;
constexpr uint8_t PGN_HARDWARE_MESSAGE  = aog_pgn::HARDWARE_MESSAGE;

// HW Message colors
constexpr uint8_t AOG_HWMSG_COLOR_GREEN  = aog_hwmsg::COLOR_GREEN;
constexpr uint8_t AOG_HWMSG_COLOR_RED    = aog_hwmsg::COLOR_RED;
constexpr uint8_t AOG_HWMSG_COLOR_YELLOW = aog_hwmsg::COLOR_YELLOW;
constexpr uint8_t AOG_HWMSG_COLOR_BLUE   = aog_hwmsg::COLOR_BLUE;
constexpr uint8_t AOG_HWMSG_DURATION_PERSIST = aog_hwmsg::DURATION_PERSIST;

// Ports
constexpr uint16_t AOG_PORT_STEER     = aog_port::STEER;
constexpr uint16_t AOG_PORT_GPS       = aog_port::GPS;
constexpr uint16_t AOG_PORT_AGIO_RECV = aog_port::AGIO_LISTEN;
constexpr uint16_t AOG_PORT_AGIO      = aog_port::AGIO_SEND;
constexpr uint16_t AOG_PORT_NMEA      = aog_port::NMEA;

// Frame sizes
constexpr size_t AOG_HEADER_SIZE  = aog_frame::HEADER_SIZE;
constexpr size_t AOG_CRC_SIZE     = aog_frame::CRC_SIZE;
constexpr size_t AOG_MAX_FRAME    = aog_frame::MAX_FRAME;
constexpr size_t AOG_MAX_PAYLOAD  = aog_frame::MAX_PAYLOAD;
constexpr size_t AOG_HWMSG_MAX_TEXT = aog_frame::HWMSG_MAX_TEXT;

// ===================================================================
// Backward-compatible function aliases
// ===================================================================

inline uint8_t aogChecksum(const uint8_t* frame, size_t frame_len) {
    return pgnChecksum(frame, frame_len);
}

inline bool aogChecksumSelfTest(void) {
    return pgnChecksumSelfTest();
}

inline size_t aogBuildFrame(uint8_t* buf, size_t buf_size,
                            uint8_t src, uint8_t pgn,
                            const void* payload, size_t payload_len) {
    return pgnBuildFrame(buf, buf_size, src, pgn, payload, payload_len);
}

inline bool aogValidateFrame(const uint8_t* frame, size_t frame_len,
                             uint8_t* out_src, uint8_t* out_pgn,
                             const uint8_t** out_payload, size_t* out_payload_len) {
    return pgnValidateFrame(frame, frame_len, out_src, out_pgn, out_payload, out_payload_len);
}

inline size_t encodeAogHelloReplySteer(uint8_t* buf, size_t buf_size,
                                        int16_t steerAngle,
                                        uint16_t sensorCounts,
                                        uint8_t switchByte) {
    return pgnEncodeHelloReplySteer(buf, buf_size, steerAngle, sensorCounts, switchByte);
}

inline size_t encodeAogHelloReplyGps(uint8_t* buf, size_t buf_size) {
    return pgnEncodeHelloReplyGps(buf, buf_size);
}

inline size_t encodeAogSubnetReply(uint8_t* buf, size_t buf_size,
                                   uint8_t src,
                                   const uint8_t ip[4],
                                   const uint8_t subnet[3]) {
    return pgnEncodeSubnetReply(buf, buf_size, src, ip, subnet);
}

inline size_t encodeAogSteerStatusOut(uint8_t* buf, size_t buf_size,
                                      int16_t actualAngleX100,
                                      int16_t headingX10,
                                      int16_t rollX10,
                                      uint8_t switchStatus,
                                      uint8_t pwmDisplay) {
    return pgnEncodeSteerStatusOut(buf, buf_size, actualAngleX100, headingX10,
                                   rollX10, switchStatus, pwmDisplay);
}

inline size_t encodeAogFromAutosteer2(uint8_t* buf, size_t buf_size,
                                      uint8_t sensorValue) {
    return pgnEncodeFromAutosteer2(buf, buf_size, sensorValue);
}

inline size_t encodeAogGpsMainOut(uint8_t* buf, size_t buf_size,
                                  const AogGpsMainOut& gps) {
    return pgnEncodeGpsMainOut(buf, buf_size, gps);
}

inline size_t encodeAogHardwareMessage(uint8_t* buf, size_t buf_size,
                                       uint8_t src,
                                       uint8_t duration,
                                       uint8_t color,
                                       const char* message) {
    return pgnEncodeHardwareMessage(buf, buf_size, src, duration, color, message);
}

inline bool tryDecodeAogHelloFromAgio(const uint8_t* payload, size_t payload_len,
                                      AogHelloFromAgio* out) {
    return pgnDecodeHelloFromAgio(payload, payload_len, out);
}

inline bool tryDecodeAogScanRequest(const uint8_t* payload, size_t payload_len) {
    return pgnDecodeScanRequest(payload, payload_len);
}

inline bool tryDecodeAogSubnetChange(const uint8_t* payload, size_t payload_len,
                                     AogSubnetChange* out) {
    return pgnDecodeSubnetChange(payload, payload_len, out);
}

inline bool tryDecodeAogSteerDataIn(const uint8_t* payload, size_t payload_len,
                                    AogSteerDataIn* out) {
    return pgnDecodeSteerDataIn(payload, payload_len, out);
}

inline bool tryDecodeAogSteerSettingsIn(const uint8_t* payload, size_t payload_len,
                                       AogSteerSettingsIn* out) {
    return pgnDecodeSteerSettingsIn(payload, payload_len, out);
}

inline bool tryDecodeAogSteerConfigIn(const uint8_t* payload, size_t payload_len,
                                      AogSteerConfigIn* out) {
    return pgnDecodeSteerConfigIn(payload, payload_len, out);
}

inline bool tryDecodeAogHardwareMessage(const uint8_t* payload, size_t payload_len,
                                        uint8_t* out_duration,
                                        uint8_t* out_color,
                                        char* out_message, size_t out_msg_size) {
    return pgnDecodeHardwareMessage(payload, payload_len, out_duration, out_color,
                                    out_message, out_msg_size);
}

inline void aogHexDump(const char* label, const uint8_t* data, size_t len) {
    pgnHexDump(label, data, len);
}
