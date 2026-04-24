/**
 * @file pgn_codec.cpp
 * @brief AgOpenGPS PGN codec implementation.
 *
 * Pure C++ – no Arduino / ESP32 headers.
 * Reference: https://github.com/AgOpenGPS-Official/Boards/blob/main/PGN.md
 */

#include "pgn_codec.h"
#include "global_state.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_PGN
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>
#include <cstring>

// ===================================================================
// Checksum — additive 8-bit, NOT a CRC polynomial
//
// Algorithm: sum bytes[2 .. frame_len-2] (excludes preamble [0,1]
// and checksum byte itself), returns low 8 bits.
//
// Identical to AgOpenGPS reference (Autosteer_UDP_v5.ino,
// UDPComm.Designer.cs).
// ===================================================================
uint8_t pgnChecksum(const uint8_t* frame, size_t frame_len) {
    if (frame_len < aog_frame::HEADER_SIZE + aog_frame::CRC_SIZE) return 0;

    size_t last_included = frame_len - 1;  // Index of the CRC byte
    uint16_t sum = 0;
    for (size_t i = 2; i < last_included; i++) {
        sum += frame[i];
    }
    return static_cast<uint8_t>(sum & 0xFF);
}

// ===================================================================
// Build a complete AOG frame
// ===================================================================
size_t pgnBuildFrame(uint8_t* buf, size_t buf_size,
                     uint8_t src, uint8_t pgn,
                     const void* payload, size_t payload_len) {
    size_t total = aog_frame::HEADER_SIZE + payload_len + aog_frame::CRC_SIZE;
    if (buf_size < total) {
        LOGE("PGN", "frame buffer too small: need %zu, have %zu", total, buf_size);
        return 0;
    }

    // Preamble
    buf[0] = AOG_PREAMBLE_1;
    buf[1] = AOG_PREAMBLE_2;

    // Header
    buf[2] = src;
    buf[3] = pgn;
    buf[4] = static_cast<uint8_t>(payload_len);

    // Payload
    if (payload_len > 0 && payload != nullptr) {
        std::memcpy(buf + aog_frame::HEADER_SIZE, payload, payload_len);
    }

    // Checksum over bytes 2..(total-2)
    buf[total - 1] = pgnChecksum(buf, total);

    return total;
}

// ===================================================================
// Check if a PGN belongs to the discovery/management path.
//
// Discovery PGNs (200, 201, 202) are sent by AgIO with static trailing
// bytes that do NOT match the additive checksum rule.  The AgIO and
// ModSim discovery receive paths do not validate checksum before
// processing.  Therefore we must skip CRC validation for these PGNs.
//
// Module hello replies (PGN == source byte, e.g. 0x7E, 0x7B, 0x79)
// are also not checksum-validated by AgIO on the receive path.
// ===================================================================
static bool pgnIsDiscovery(uint8_t pgn, uint8_t src) {
    // Discovery PGNs from AgIO
    if (pgn == 0xC8 || pgn == 0xC9 || pgn == 0xCA) return true;

    // Module hello replies: PGN == src byte
    // 0x7E = steer, 0x7B = machine, 0x79 = IMU, 0x78 = GPS
    if (pgn == src && (pgn == 0x7E || pgn == 0x7B || pgn == 0x79 || pgn == 0x78))
        return true;

    return false;
}

// ===================================================================
// Validate a received frame: preamble, bounds, CRC
// ===================================================================
bool pgnValidateFrame(const uint8_t* frame, size_t frame_len,
                      uint8_t* out_src, uint8_t* out_pgn,
                      const uint8_t** out_payload, size_t* out_payload_len) {
    // Minimum: preamble(2) + src(1) + pgn(1) + len(1) + crc(1) = 6
    if (frame_len < 6) return false;

    // Check preamble
    if (frame[0] != AOG_PREAMBLE_1 || frame[1] != AOG_PREAMBLE_2) return false;

    // Extract header fields
    uint8_t src = frame[2];
    uint8_t pgn = frame[3];
    uint8_t len = frame[4];

    // Bounds check: header(5) + payload(len) + crc(1)
    if (frame_len < static_cast<size_t>(aog_frame::HEADER_SIZE + len + aog_frame::CRC_SIZE))
        return false;
    if (len > aog_frame::MAX_PAYLOAD) return false;

    // CRC check — skip for discovery/management PGNs
    // (AgIO sends these with static trailing bytes that don't match)
    if (!pgnIsDiscovery(pgn, src)) {
        size_t crc_idx = aog_frame::HEADER_SIZE + len;
        uint8_t expected_crc = pgnChecksum(frame, crc_idx + aog_frame::CRC_SIZE);
        if (frame[crc_idx] != expected_crc) {
            // Rate-limit CRC mismatch logs (max once per 10s)
            static uint32_t s_last_crc_log_ms = 0;
            uint32_t now = hal_millis();
            if (now - s_last_crc_log_ms >= 10000) {
                s_last_crc_log_ms = now;
                // Include hex dump for diagnosis
                char hexbuf[128];
                char* p = hexbuf;
                for (size_t i = 0; i < frame_len && (p - hexbuf) < static_cast<int>(sizeof(hexbuf)) - 4; i++) {
                    p += snprintf(p, 4, "%02X ", frame[i]);
                }
                LOGE("PGN", "CRC mismatch: got 0x%02X, exp 0x%02X (Src=%u PGN=0x%02X) [%s]",
                        frame[crc_idx], expected_crc, src, pgn, hexbuf);
            }
            return false;
        }
    }

    // Fill outputs
    if (out_src)         *out_src = src;
    if (out_pgn)         *out_pgn = pgn;
    if (out_payload)     *out_payload = frame + aog_frame::HEADER_SIZE;
    if (out_payload_len) *out_payload_len = len;

    return true;
}

// ===================================================================
// Self-test implementation
// ===================================================================
bool pgnChecksumSelfTest(void) {
    // Build a known frame: PGN 253, 8 bytes zero payload
    // Expected: 0x80 0x81 0x7E 0xFD 0x08 [8 zeros] checksum
    // Checksum = 0x7E + 0xFD + 0x08 + 0*8 = 0x17B, low 8 bits = 0x7B
    uint8_t test_payload[8] = {0};
    uint8_t tx[64], rx[64];
    size_t tx_len = pgnBuildFrame(tx, sizeof(tx), aog_src::STEER, aog_pgn::STEER_STATUS_OUT,
                                   test_payload, 8);
    if (tx_len != 14) return false;
    if (tx[0] != 0x80 || tx[1] != 0x81) return false;
    if (tx[tx_len - 1] != 0x7B) return false;

    // Validate the frame we just built
    uint8_t v_src = 0, v_pgn = 0;
    const uint8_t* v_pay = nullptr;
    size_t v_plen = 0;
    if (!pgnValidateFrame(tx, tx_len, &v_src, &v_pgn, &v_pay, &v_plen)) return false;
    if (v_src != aog_src::STEER || v_pgn != aog_pgn::STEER_STATUS_OUT || v_plen != 8)
        return false;

    // Corrupt the checksum and verify it fails
    std::memcpy(rx, tx, tx_len);
    rx[tx_len - 1] ^= 0xFF;
    if (pgnValidateFrame(rx, tx_len, &v_src, &v_pgn, &v_pay, &v_plen)) return false;

    // PGN 250 keeps an 8-byte payload for AgOpenGPS compatibility.
    tx_len = pgnEncodeFromAutosteer2(tx, sizeof(tx), 127);
    if (tx_len != 14) return false;
    if (!pgnValidateFrame(tx, tx_len, &v_src, &v_pgn, &v_pay, &v_plen)) return false;
    if (v_src != aog_src::STEER || v_pgn != aog_pgn::FROM_AUTOSTEER_2 || v_plen != 8)
        return false;
    if (v_pay[0] != 127) return false;
    for (size_t i = 1; i < 8; i++) {
        if (v_pay[i] != 0) return false;
    }

    // Test: Discovery PGN 200 (Hello from AgIO) with WRONG checksum
    // AgIO sends: [0x80, 0x81, 0x7F, 200, 3, 56, 0, 0, 0x47]
    // The trailing byte 0x47 does NOT match the additive checksum (0x88),
    // but the frame must still be accepted because it's a discovery PGN.
    uint8_t discovery_frame[] = { 0x80, 0x81, 0x7F, 0xC8, 0x03, 56, 0x00, 0x00, 0x47 };
    if (!pgnValidateFrame(discovery_frame, sizeof(discovery_frame),
                          &v_src, &v_pgn, &v_pay, &v_plen)) return false;
    if (v_src != 0x7F || v_pgn != 0xC8 || v_plen != 3) return false;

    // Test: Discovery PGN 202 (Scan Request) with arbitrary trailing byte
    uint8_t scan_frame[] = { 0x80, 0x81, 0x7F, 0xCA, 0x03, 0xCA, 0xCA, 0x05, 0xFF };
    if (!pgnValidateFrame(scan_frame, sizeof(scan_frame),
                          &v_src, &v_pgn, &v_pay, &v_plen)) return false;
    if (v_src != 0x7F || v_pgn != 0xCA || v_plen != 3) return false;

    return true;
}

// ===================================================================
// PGN Name Table
// ===================================================================
struct PgnNameEntry {
    uint8_t pgn;
    const char* name;
};

static const PgnNameEntry s_pgn_names[] = {
    { 0xC8, "HelloFromAgIO" },
    { 0xC9, "SubnetChange" },
    { 0xCA, "ScanRequest" },
    { 0xCB, "SubnetReply" },
    { 0x7E, "HelloReplySteer" },
    { 0x78, "HelloReplyGps" },
    { 0xFE, "SteerDataIn" },
    { 0xFD, "SteerStatusOut" },
    { 0xFC, "SteerSettingsIn" },
    { 0xFB, "SteerConfigIn" },
    { 0xFA, "FromAutosteer2" },
    { 0xD6, "GpsMainOut" },
    { 0xEF, "MachineDataIn" },
    { 0xEE, "MachineConfigIn" },
    { 0xDD, "HardwareMessage" },
};
static constexpr size_t s_pgn_name_count = sizeof(s_pgn_names) / sizeof(s_pgn_names[0]);

const char* pgnGetName(uint8_t pgn) {
    for (size_t i = 0; i < s_pgn_name_count; i++) {
        if (s_pgn_names[i].pgn == pgn) return s_pgn_names[i].name;
    }
    return "Unknown";
}

// ===================================================================
// Encoder: Hello Reply – Steer (Src=0x7E, PGN=0x7E, Len=5)
// ===================================================================
size_t pgnEncodeHelloReplySteer(uint8_t* buf, size_t buf_size,
                                int16_t steerAngle,
                                uint16_t sensorCounts,
                                uint8_t switchByte) {
    AogHelloReplySteer reply;
    reply.steerAngle   = steerAngle;
    reply.sensorCounts = sensorCounts;
    reply.switchByte   = switchByte;
    return pgnBuildFrame(buf, buf_size, aog_src::STEER, aog_pgn::HELLO_REPLY_STEER,
                         &reply, sizeof(reply));
}

// ===================================================================
// Encoder: Hello Reply – GPS (Src=0x78, PGN=0x78, Len=5)
// ===================================================================
size_t pgnEncodeHelloReplyGps(uint8_t* buf, size_t buf_size) {
    AogHelloReplyGps reply;
    std::memset(&reply, 0, sizeof(reply));
    return pgnBuildFrame(buf, buf_size, aog_src::GPS_REPLY, aog_pgn::HELLO_REPLY_GPS,
                         &reply, sizeof(reply));
}

// ===================================================================
// Encoder: Subnet Reply (Src=module, PGN=0xCB, Len=7)
// ===================================================================
size_t pgnEncodeSubnetReply(uint8_t* buf, size_t buf_size,
                            uint8_t src,
                            const uint8_t ip[4],
                            const uint8_t subnet[3]) {
    AogSubnetReply reply;
    std::memcpy(reply.ip, ip, 4);
    std::memcpy(reply.subnet, subnet, 3);
    return pgnBuildFrame(buf, buf_size, src, aog_pgn::SUBNET_REPLY,
                         &reply, sizeof(reply));
}

// ===================================================================
// Encoder: Steer Status Out (PGN 0xFD, Src=0x7E, Len=8)
// ===================================================================
size_t pgnEncodeSteerStatusOut(uint8_t* buf, size_t buf_size,
                               int16_t actualAngleX100,
                               int16_t headingX10,
                               int16_t rollX10,
                               uint8_t switchStatus,
                               uint8_t pwmDisplay) {
    AogSteerStatusOut status;
    status.actualSteerAngle = actualAngleX100;
    status.imuHeading       = headingX10;
    status.imuRoll          = rollX10;
    status.switchStatus     = switchStatus;
    status.pwmDisplay       = pwmDisplay;
    return pgnBuildFrame(buf, buf_size, aog_src::STEER, aog_pgn::STEER_STATUS_OUT,
                         &status, sizeof(status));
}

// ===================================================================
// Encoder: From Autosteer 2 (PGN 0xFA, Src=0x7E, Len=8)
// ===================================================================
size_t pgnEncodeFromAutosteer2(uint8_t* buf, size_t buf_size,
                               uint8_t sensorValue) {
    AogFromAutosteer2 msg;
    std::memset(&msg, 0, sizeof(msg));
    msg.sensorValue = sensorValue;
    return pgnBuildFrame(buf, buf_size, aog_src::STEER, aog_pgn::FROM_AUTOSTEER_2,
                         &msg, sizeof(msg));
}

// ===================================================================
// Encoder: GPS Main Out (PGN 0xD6, Src=0x7C, Len=51)
// ===================================================================
size_t pgnEncodeGpsMainOut(uint8_t* buf, size_t buf_size,
                           const AogGpsMainOut& gps) {
    return pgnBuildFrame(buf, buf_size, aog_src::GPS, aog_pgn::GPS_MAIN_OUT,
                         &gps, sizeof(gps));
}

// ===================================================================
// Encoder: Hardware Message (PGN 0xDD, variable length)
// Payload: Duration(1) | Color(1) | Message(null-terminated)
// ===================================================================
size_t pgnEncodeHardwareMessage(uint8_t* buf, size_t buf_size,
                                uint8_t src,
                                uint8_t duration,
                                uint8_t color,
                                const char* message) {
    if (!message) return 0;

    size_t msg_len = std::strlen(message);
    if (msg_len == 0) return 0;
    if (msg_len > aog_frame::HWMSG_MAX_TEXT) msg_len = aog_frame::HWMSG_MAX_TEXT;

    // Build payload first, then use pgnBuildFrame
    uint8_t payload[aog_frame::HWMSG_MAX_TEXT + 2];
    size_t payload_len = 0;
    payload[payload_len++] = duration;
    payload[payload_len++] = color;
    std::memcpy(payload + payload_len, message, msg_len);
    payload_len += msg_len;
    payload[payload_len++] = '\0';  // null terminator

    return pgnBuildFrame(buf, buf_size, src, aog_pgn::HARDWARE_MESSAGE,
                         payload, payload_len);
}

// ===================================================================
// Decoder: Hello From AgIO (PGN 200, Len=3)
// ===================================================================
bool pgnDecodeHelloFromAgio(const uint8_t* payload, size_t payload_len,
                            AogHelloFromAgio* out) {
    if (!payload || payload_len < sizeof(AogHelloFromAgio)) return false;

    AogHelloFromAgio msg;
    std::memcpy(&msg, payload, sizeof(msg));

    if (out) *out = msg;
    LOGI("PGN", "Hello from AgIO (module=%u, ver=%u)",
            (unsigned)msg.moduleId, (unsigned)msg.agioVersion);
    return true;
}

// ===================================================================
// Decoder: Scan Request (PGN 202)
// ===================================================================
bool pgnDecodeScanRequest(const uint8_t* payload, size_t payload_len) {
    if (!payload || payload_len < 1) return false;
    LOGI("PGN", "Scan request received (len=%zu)", payload_len);
    return true;
}

// ===================================================================
// Decoder: Subnet Change (PGN 201, Len=5)
// ===================================================================
bool pgnDecodeSubnetChange(const uint8_t* payload, size_t payload_len,
                           AogSubnetChange* out) {
    if (!payload || payload_len < sizeof(AogSubnetChange)) return false;

    AogSubnetChange msg;
    std::memcpy(&msg, payload, sizeof(msg));

    if (out) *out = msg;
    LOGI("PGN", "Subnet change -> IP=%u.%u.%u.xxx",
            (unsigned)msg.ip_one, (unsigned)msg.ip_two, (unsigned)msg.ip_three);
    return true;
}

// ===================================================================
// Decoder: Steer Data In (PGN 254, Len=8)
// ===================================================================
bool pgnDecodeSteerDataIn(const uint8_t* payload, size_t payload_len,
                          AogSteerDataIn* out) {
    if (!payload || payload_len < sizeof(AogSteerDataIn)) return false;

    AogSteerDataIn msg;
    std::memcpy(&msg, payload, sizeof(msg));

    if (out) *out = msg;
    LOGI("PGN", "SteerDataIn speed=%d angle=%d status=0x%02X",
            (int)msg.speed, (int)msg.steerAngle, (int)msg.status);
    return true;
}

// ===================================================================
// Decoder: Steer Settings In (PGN 252, Len=8)
// ===================================================================
bool pgnDecodeSteerSettingsIn(const uint8_t* payload, size_t payload_len,
                              AogSteerSettingsIn* out) {
    if (!payload || payload_len < sizeof(AogSteerSettingsIn)) return false;

    AogSteerSettingsIn msg;
    std::memcpy(&msg, payload, sizeof(msg));

    if (out) *out = msg;
    LOGI("PGN", "SteerSettings Kp=%u hiPWM=%u loPWM=%u minPWM=%u cnt=%u off=%d ack=%u",
            (unsigned)msg.kp, (unsigned)msg.highPWM, (unsigned)msg.lowPWM,
            (unsigned)msg.minPWM, (unsigned)msg.countsPerDegree,
            (int)msg.wasOffset, (unsigned)msg.ackerman);
    return true;
}

// ===================================================================
// Decoder: Steer Config In (PGN 251, Len=8)
// ===================================================================
bool pgnDecodeSteerConfigIn(const uint8_t* payload, size_t payload_len,
                            AogSteerConfigIn* out) {
    if (!payload || payload_len < sizeof(AogSteerConfigIn)) return false;

    AogSteerConfigIn msg;
    std::memcpy(&msg, payload, sizeof(msg));

    if (out) *out = msg;
    return true;
}

// ===================================================================
// Decoder: Machine Data In (PGN 239, Len=8)
// ===================================================================
bool pgnDecodeMachineDataIn(const uint8_t* payload, size_t payload_len,
                            AogMachineDataIn* out) {
    if (!payload || payload_len < sizeof(AogMachineDataIn)) return false;

    AogMachineDataIn msg;
    std::memcpy(&msg, payload, sizeof(msg));

    if (out) *out = msg;
    LOGI("PGN", "MachineData speed=%u", (unsigned)msg.speed);
    return true;
}

// ===================================================================
// Decoder: Machine Config In (PGN 238, Len=3)
// ===================================================================
bool pgnDecodeMachineConfigIn(const uint8_t* payload, size_t payload_len,
                              AogMachineConfigIn* out) {
    if (!payload || payload_len < sizeof(AogMachineConfigIn)) return false;

    AogMachineConfigIn msg;
    std::memcpy(&msg, payload, sizeof(msg));

    if (out) *out = msg;
    LOGI("PGN", "MachineConfig raise=%u lower=%u hyd=%u",
            (unsigned)msg.raiseTime, (unsigned)msg.lowerTime, (unsigned)msg.enableHyd);
    return true;
}

// ===================================================================
// Decoder: Hardware Message (PGN 221, variable length)
// ===================================================================
bool pgnDecodeHardwareMessage(const uint8_t* payload, size_t payload_len,
                              uint8_t* out_duration,
                              uint8_t* out_color,
                              char* out_message, size_t out_msg_size) {
    // Minimum: duration(1) + color(1) + null terminator(1) = 3
    if (!payload || payload_len < 3) return false;

    uint8_t duration = payload[0];
    uint8_t color    = payload[1];

    // Message starts at payload[2], null-terminated
    const char* msg = reinterpret_cast<const char*>(payload + 2);

    // Ensure null terminator exists within payload
    size_t max_msg_len = payload_len - 2;
    size_t msg_len = 0;
    while (msg_len < max_msg_len && msg[msg_len] != '\0') {
        msg_len++;
    }

    if (out_duration) *out_duration = duration;
    if (out_color)    *out_color = color;

    if (out_message && out_msg_size > 0) {
        size_t copy_len = msg_len < (out_msg_size - 1) ? msg_len : (out_msg_size - 1);
        std::memcpy(out_message, msg, copy_len);
        out_message[copy_len] = '\0';
    }

    LOGI("PGN", "HW Message dur=%u color=%u msg=\"%s\"", duration, color, msg);
    return true;
}

// ===================================================================
// Utility: Hex dump to log
// ===================================================================
void pgnHexDump(const char* label, const uint8_t* data, size_t len) {
    LOGI("PGN", "%s (%zu bytes):", label ? label : "PGN dump", len);
    for (size_t i = 0; i < len; i += 16) {
        char line[80];
        int pos = 0;
        pos += snprintf(line + pos, sizeof(line) - pos, "  %04zx: ", i);
        for (size_t j = i; j < i + 16 && j < len; j++) {
            pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[j]);
        }
        LOGI("PGN", "%s", line);
    }
}
