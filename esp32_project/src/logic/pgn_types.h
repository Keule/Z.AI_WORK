/**
 * @file pgn_types.h
 * @brief AgOpenGPS PGN type definitions — all constants, structs, enums.
 *
 * Single source of truth for:
 *   - Preamble bytes
 *   - Source IDs
 *   - PGN numbers (grouped by category)
 *   - PGN struct layouts (packed, with static_assert size checks)
 *   - Network constants (ports, config)
 *   - Hardware message color codes
 *   - Frame size limits
 *
 * Reference: https://github.com/AgOpenGPS-Official/Boards/blob/main/PGN.md
 *
 * This header has NO dependencies beyond <cstdint> and <cstddef>.
 * No Arduino/ESP32 headers.
 */

#pragma once

#include <cstddef>
#include <cstdint>

// ===================================================================
// AOG Preamble
// ===================================================================
constexpr uint8_t AOG_PREAMBLE_1 = 0x80;
constexpr uint8_t AOG_PREAMBLE_2 = 0x81;

// ===================================================================
// Source IDs (Src byte in frame header)
// ===================================================================
namespace aog_src {
    constexpr uint8_t AGIO      = 0x7F;  // 127 – AgIO application
    constexpr uint8_t STEER     = 0x7E;  // 126 – Steer module (this board)
    constexpr uint8_t MACHINE   = 0x7B;  // 123 – Machine module
    constexpr uint8_t GPS       = 0x7C;  // 124 – GPS module
    constexpr uint8_t GPS_REPLY = 0x78;  // 120 – GPS hello reply
}

// ===================================================================
// PGN Numbers — grouped by functional category
// ===================================================================
namespace aog_pgn {

    // -- Network Discovery (AgIO -> Modules) --
    constexpr uint8_t HELLO_FROM_AGIO = 0xC8;  // 200  AgIO hello broadcast
    constexpr uint8_t SUBNET_CHANGE   = 0xC9;  // 201  Subnet change request
    constexpr uint8_t SCAN_REQUEST    = 0xCA;  // 202  Scan / discover modules
    constexpr uint8_t SUBNET_REPLY    = 0xCB;  // 203  Subnet info reply

    // -- Hello Replies (Module -> AgIO, PGN field = Src for replies) --
    constexpr uint8_t HELLO_REPLY_STEER = 0x7E;  // 126  Steer module hello reply
    constexpr uint8_t HELLO_REPLY_GPS   = 0x78;  // 120  GPS module hello reply

    // -- Steering (bidirectional) --
    constexpr uint8_t STEER_DATA_IN     = 0xFE;  // 254  AgIO -> steer (speed, angle, switches)
    constexpr uint8_t STEER_STATUS_OUT  = 0xFD;  // 253  Steer -> AgIO (angle, heading, switches)
    constexpr uint8_t STEER_SETTINGS_IN = 0xFC;  // 252  AgIO -> steer (PID params)
    constexpr uint8_t STEER_CONFIG_IN   = 0xFB;  // 251  AgIO -> steer (hardware config bits)
    constexpr uint8_t FROM_AUTOSTEER_2  = 0xFA;  // 250  Steer -> AgIO (sensor raw data)

    // -- GPS --
    constexpr uint8_t GPS_MAIN_OUT = 0xD6;  // 214  GPS -> AgIO (position, heading)

    // -- Machine --
    constexpr uint8_t MACHINE_DATA_IN   = 0xEF;  // 239  AgIO -> machine (speed)
    constexpr uint8_t MACHINE_CONFIG_IN = 0xEE;  // 238  AgIO -> machine (hydraulic config)

    // -- Diagnostics --
    constexpr uint8_t HARDWARE_MESSAGE = 0xDD;  // 221  Hardware status message (bidirectional)
}

// ===================================================================
// Hardware Message Color Codes (for PGN 0xDD)
// ===================================================================
namespace aog_hwmsg {
    constexpr uint8_t COLOR_GREEN  = 0;  // OK / info
    constexpr uint8_t COLOR_RED    = 1;  // Error / critical
    constexpr uint8_t COLOR_YELLOW = 2;  // Warning
    constexpr uint8_t COLOR_BLUE   = 3;  // Informational

    /// Special duration: message persists until updated or dismissed
    constexpr uint8_t DURATION_PERSIST = 0;
}

// ===================================================================
// UDP Port Constants
// ===================================================================
namespace aog_port {
    /// Module listen port — AgIO sends TO this port
    constexpr uint16_t AGIO_LISTEN = 8888;

    /// AgIO listen port — we send TO this port
    constexpr uint16_t AGIO_SEND = 9999;

    /// Steer module source port (we send FROM this)
    constexpr uint16_t STEER = 5126;

    /// GPS module source port
    constexpr uint16_t GPS = 5124;

    /// Machine module source port
    constexpr uint16_t MACHINE = 5127;

    /// NMEA data port (unused in steer firmware)
    constexpr uint16_t NMEA = 29999;

    /// RTCM correction stream listen port (AgIO/NTRIP bridge -> module).
    /// This port is intentionally separate from AOG PGN traffic.
    constexpr uint16_t RTCM_LISTEN = 2233;
}

// ===================================================================
// Frame Size Limits
// ===================================================================
namespace aog_frame {
    constexpr size_t HEADER_SIZE  = 5;    // 0x80 + 0x81 + Src + PGN + Len
    constexpr size_t CRC_SIZE     = 1;
    constexpr size_t MAX_FRAME    = 256;  // Safety limit for receive buffer
    constexpr size_t MAX_PAYLOAD  = MAX_FRAME - HEADER_SIZE - CRC_SIZE;  // 250
    constexpr size_t HWMSG_MAX_TEXT = 200;  // Max text length for hardware messages
}

// ===================================================================
// Network Configuration (runtime-adjustable via PGN 0xC9)
// ===================================================================
struct AogNetworkConfig {
    uint8_t  dest_ip[4]  = {192, 168, 1, 255};  // Broadcast default
    uint16_t dest_port   = aog_port::AGIO_SEND;
};

/// Global network config instance (defined in net.cpp)
extern AogNetworkConfig g_net_cfg;

// ===================================================================
// Frame Header (5 bytes on the wire)
// ===================================================================
struct __attribute__((packed)) AogFrameHeader {
    uint8_t  preamble[2];   // 0x80, 0x81
    uint8_t  src;
    uint8_t  pgn;
    uint8_t  len;           // Payload byte count following this header
};

// ===================================================================
// PGN Structs — Network Discovery
// ===================================================================

/// PGN 200: Hello from AgIO (Src=0x7F)
/// Payload: ModuleID(1), Spare(1), AgIOVersion(1)
struct __attribute__((packed)) AogHelloFromAgio {
    uint8_t  moduleId;       // Which module this hello targets (= 0x7F)
    uint8_t  spare;
    uint8_t  agioVersion;    // AgIO version number
};
static_assert(sizeof(AogHelloFromAgio) == 3, "AogHelloFromAgio must be 3 bytes");

/// PGN 202: Scan Request (Src=0x7F)
/// Payload: ScanByte1(1), ScanByte2(1), NumberOfModules(1)
struct __attribute__((packed)) AogScanRequest {
    uint8_t  scanByte1;       // = 0xCA (repeated PGN)
    uint8_t  scanByte2;       // = 0xCA (repeated PGN)
    uint8_t  numberOfModules; // Number of modules to scan (typically 5)
};
static_assert(sizeof(AogScanRequest) == 3, "AogScanRequest must be 3 bytes");

/// PGN 201: Subnet Change (Src=0x7F)
/// Payload: ID1(1), ID2(1), IP_One(1), IP_Two(1), IP_Three(1)
struct __attribute__((packed)) AogSubnetChange {
    uint8_t  id1;            // = 0xC9 (repeated PGN)
    uint8_t  id2;            // = 0xC9 (repeated PGN)
    uint8_t  ip_one;         // First octet of destination IP
    uint8_t  ip_two;         // Second octet
    uint8_t  ip_three;       // Third octet (4th = 255 broadcast)
};
static_assert(sizeof(AogSubnetChange) == 5, "AogSubnetChange must be 5 bytes");

/// PGN 203: Subnet Reply (Src=module)
/// Payload: IP[4] + Subnet[3]
struct __attribute__((packed)) AogSubnetReply {
    uint8_t  ip[4];          // Module's full IP address
    uint8_t  subnet[3];      // Subnet mask first 3 octets (e.g. 255.255.255)
};
static_assert(sizeof(AogSubnetReply) == 7, "AogSubnetReply must be 7 bytes");

// ===================================================================
// PGN Structs — Hello Replies
// ===================================================================

/// PGN 126: Steer Hello Reply (Src=0x7E)
/// Payload: SteerAngle(i16 LE, ×100), SensorCounts(u16 LE), SwitchByte(1)
struct __attribute__((packed)) AogHelloReplySteer {
    int16_t  steerAngle;     // Current steer angle [degrees × 100], signed LE
    uint16_t sensorCounts;   // Raw sensor counts
    uint8_t  switchByte;     // Switch status bits
};
static_assert(sizeof(AogHelloReplySteer) == 5, "AogHelloReplySteer must be 5 bytes");

/// PGN 120: GPS Hello Reply (Src=0x78)
/// Payload: Reserved(5)
struct __attribute__((packed)) AogHelloReplyGps {
    uint8_t  reserved[5];
};
static_assert(sizeof(AogHelloReplyGps) == 5, "AogHelloReplyGps must be 5 bytes");

// ===================================================================
// PGN Structs — Steering
// ===================================================================

/// PGN 254: Steer Data In (Src=0x7F -> Steer)
/// Total payload: 8 bytes
///
/// Field layout (from AgOpenGPS C# reference CPGN_FE):
///   [0-1] Speed       int16 LE  [km/h × 10]
///   [2]   Status      uint8     bitfield (bit0=work, bit1=steer, bit2=on)
///   [3-4] SteerAngle  int16 LE  [degrees × 100]
///   [5]   XTE         uint8     cross track error
///   [6]   SC1to8      uint8     section control 1-8
///   [7]   SC9to16     uint8     section control 9-16
struct __attribute__((packed)) AogSteerDataIn {
    int16_t  speed;           // [km/h × 10], signed LE
    uint8_t  status;          // bit0=workSwitch, bit1=steerSwitch, bit2=on
    int16_t  steerAngle;      // [degrees × 100], signed LE
    uint8_t  xte;             // Cross track error
    uint8_t  sectionControl1; // SC1..SC8
    uint8_t  sectionControl2; // SC9..SC16
};
static_assert(sizeof(AogSteerDataIn) == 8, "AogSteerDataIn must be 8 bytes");

/// PGN 253: Steer Status Out (Src=0x7E -> AgIO)
/// Total payload: 8 bytes
///
/// Field layout (from AgOpenGPS C# reference CPGN_FD):
///   [0-1] ActualSteerAngle  int16 LE  [degrees × 100]
///   [2-3] IMUHeading        int16 LE  [degrees × 10]
///   [4-5] IMURoll           int16 LE  [degrees × 10]
///   [6]   SwitchStatus      uint8     bitfield
///   [7]   PWMDisplay        uint8     current PWM (0-255)
struct __attribute__((packed)) AogSteerStatusOut {
    int16_t  actualSteerAngle;  // [degrees × 100], signed LE
    int16_t  imuHeading;        // [degrees × 10], signed LE; 9999 = not available
    int16_t  imuRoll;           // [degrees × 10], signed LE; 8888 = not available
    uint8_t  switchStatus;      // bit0=work, bit1=steer, bit7=safety
    uint8_t  pwmDisplay;        // Current PWM value (0-255)
};
static_assert(sizeof(AogSteerStatusOut) == 8, "AogSteerStatusOut must be 8 bytes");

/// PGN 252: Steer Settings In (Src=0x7F -> Steer)
/// Total payload: 8 bytes
///
/// Field layout (from AgOpenGPS C# reference CPGN_FC):
///   [0]   Kp              uint8     proportional gain (raw value)
///   [1]   HighPWM         uint8     maximum actuator PWM
///   [2]   LowPWM          uint8     deadband / no-action band
///   [3]   MinPWM          uint8     minimum actuator PWM for instant on
///   [4]   CountsPerDegree uint8     steer angle sensor counts per degree
///   [5-6] WASOffset       int16 LE  sensor zero offset
///   [7]   Ackerman        uint8     Ackerman correction (actual = value / 100.0)
struct __attribute__((packed)) AogSteerSettingsIn {
    uint8_t  kp;              // [0] proportional gain
    uint8_t  highPWM;         // [1] maximum actuator PWM
    uint8_t  lowPWM;          // [2] deadband (no-action PWM band)
    uint8_t  minPWM;          // [3] minimum actuator PWM for instant on
    uint8_t  countsPerDegree; // [4] steer angle sensor counts per degree
    int16_t  wasOffset;       // [5-6] sensor zero offset (LE)
    uint8_t  ackerman;        // [7] Ackerman correction factor (×100)
};
static_assert(sizeof(AogSteerSettingsIn) == 8, "AogSteerSettingsIn must be 8 bytes");

/// PGN 251: Steer Config In (Src=0x7F -> Steer)
/// Total payload: 8 bytes
///
/// Field layout (from AgOpenGPS C# reference CPGN_FB):
///   [0]   Set0          uint8  config bits:
///     bit0: InvertWAS, bit1: RelayActiveHigh, bit2: MotorDriveDirection,
///     bit3: SingleInputWAS, bit4: CytronDriver, bit5: SteerSwitch,
///     bit6: SteerButton, bit7: ShaftEncoder
///   [1]   MaxPulse     uint8  pulse count max threshold
///   [2]   MinSpeed     uint8  minimum speed for steering
///   [3]   AckermanFix  uint8  Ackerman fix percentage
///   [4-7] Reserved     uint8  spare bytes
struct __attribute__((packed)) AogSteerConfigIn {
    uint8_t  set0;             // [0] configuration bits
    uint8_t  maxPulse;         // [1] pulse count max threshold
    uint8_t  minSpeed;         // [2] minimum speed for steering
    uint8_t  ackermanFix;      // [3] Ackerman fix percentage
    uint8_t  reserved[4];      // [4-7] spare bytes
};
static_assert(sizeof(AogSteerConfigIn) == 8, "AogSteerConfigIn must be 8 bytes");

/// PGN 250: From Autosteer 2 (Src=0x7E -> AgIO)
/// Total payload: 8 bytes for compatibility. Byte 0 is WAS scaled 0..255.
struct __attribute__((packed)) AogFromAutosteer2 {
    uint8_t  sensorValue;       // Calibrated WAS 0..255, raw low byte fallback
    uint8_t  reserved[7];       // Unused
};
static_assert(sizeof(AogFromAutosteer2) == 8, "AogFromAutosteer2 must be 8 bytes");

// ===================================================================
// PGN Structs — GPS
// ===================================================================

/// PGN 214: GPS Main Out (Src=0x7C -> AgIO)
/// Total payload: 51 bytes
struct __attribute__((packed)) AogGpsMainOut {
    int32_t  longitude;         // [0-3]   [degrees × 1e7]
    int32_t  latitude;          // [4-7]   [degrees × 1e7]
    int16_t  heading;           // [8-9]   True heading [degrees × 16]
    int16_t  dualHeading;       // [10-11] Dual heading [degrees × 16]
    uint16_t speed;             // [12-13] Speed [mm/s]
    int16_t  roll;              // [14-15] Roll [degrees × 16]
    int32_t  altitude;          // [16-19] Altitude [mm]
    uint8_t  satCount;          // [20]    Satellites tracked
    uint8_t  fixQuality;        // [21]    Fix quality (0=none,1=GPS,2=DGPS,4=RTK Fix,5=RTK Float)
    int16_t  hdop;              // [22-23] HDOP × 100
    int16_t  age;               // [24-25] Differential age × 100 [ms] (signed, saturating)
    int16_t  imuHeading;        // [26-27] IMU heading [degrees × 16]
    int16_t  imuRoll;           // [28-29] IMU roll [degrees × 16]
    int16_t  imuPitch;          // [30-31] IMU pitch [degrees × 16]
    int16_t  imuYawRate;        // [32-33] IMU yaw rate
    int16_t  reserved1;         // [34-35] Reserved
    uint32_t relay;             // [36-39] Section/relay bits
    uint8_t  section1_8;        // [40]    Section control 1-8
    uint8_t  section9_16;       // [41]    Section control 9-16
    uint16_t year;              // [42-43] Year
    uint8_t  month;             // [44]    Month
    uint8_t  day;               // [45]    Day
    uint8_t  hour;              // [46]    Hour
    uint8_t  minute;            // [47]    Minute
    uint8_t  second;            // [48]    Second
    uint8_t  ms_hi;             // [49]    Milliseconds high
    uint8_t  ms_lo;             // [50]    Milliseconds low
};
static_assert(sizeof(AogGpsMainOut) == 51, "AogGpsMainOut must be exactly 51 bytes");

// ===================================================================
// PGN Structs — Machine
// ===================================================================

/// PGN 239: Machine Data In (Src=0x7F -> Machine)
/// Total payload: 8 bytes
struct __attribute__((packed)) AogMachineDataIn {
    uint8_t  uTurn;             // [0] U-turn status
    uint8_t  tree;              // [1] Tree row status
    uint8_t  reserved1;         // [2]
    uint8_t  hydLift;           // [3] Hydraulic lift
    uint8_t  speed;             // [4] Machine speed
    uint8_t  reserved[3];       // [5-7]
};
static_assert(sizeof(AogMachineDataIn) == 8, "AogMachineDataIn must be 8 bytes");

/// PGN 238: Machine Config In (Src=0x7F -> Machine)
/// Total payload: 3 bytes
struct __attribute__((packed)) AogMachineConfigIn {
    uint8_t  raiseTime;         // [0] Hydraulic raise time
    uint8_t  lowerTime;         // [1] Hydraulic lower time
    uint8_t  enableHyd;         // [2] Hydraulics enable
};
static_assert(sizeof(AogMachineConfigIn) == 3, "AogMachineConfigIn must be 3 bytes");

// ===================================================================
// PGN Structs — Hardware Message (variable length)
// ===================================================================
// PGN 221: Hardware Message
// Payload: Duration(1) | Color(1) | Message(null-terminated ASCII)
// No fixed struct — variable length, handled by encoder/decoder directly.
