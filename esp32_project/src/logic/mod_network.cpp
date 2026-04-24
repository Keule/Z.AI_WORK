/**
 * @file mod_network.cpp
 * @brief Protocol module: PGN Codec RX/TX over UDP (ModuleId::NETWORK).
 *
 * This module handles ALL PGN encoding/decoding for communication with
 * AgOpenGPS. Physical transport is delegated to ETH and/or WIFI modules.
 *
 * Migrated from net.h/net.cpp. Contains:
 *   - PGN frame receive + dispatch (PGN 200/201/202/221/251/252/254)
 *   - PGN frame send (PGN 250/253/214) at ~10 Hz
 *   - RTCM ring buffer for GNSS correction forwarding
 *   - AgIO module hello/subnet replies
 *   - Steer settings/config application from AgIO
 *
 * Reference: https://github.com/AgOpenGPS-Official/Boards/blob/main/PGN.md
 */

#include "mod_network.h"
#include "hal/hal.h"
#include "nvs_config.h"
#include "mod_steer.h"
#include "dependency_policy.h"
#include "diag.h"
#include "global_state.h"
#include "runtime_config.h"
#include "setup_wizard.h"
#include "pgn_codec.h"
#include "pgn_registry.h"

// Forward declaration from op_mode.h (avoid OpMode name conflict with module_interface.h)
extern "C" bool opModeIsPausedStatusBit(void);

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_NET
#include "esp_log.h"
#include "log_ext.h"

#include <climits>
#include <cctype>
#include <cstdio>
#include <cstring>

static const char* const TAG = "MOD_NETWORK";

// ===================================================================
// Error codes
// ===================================================================
static constexpr int32_t ERR_NO_TRANSPORT = 1;

// ===================================================================
// Freshness timeout for health check
// ===================================================================
static constexpr uint32_t FRESHNESS_MS = 5000;

// ===================================================================
// Internal state
// ===================================================================
static ModState s_state;
static ModuleRuntime* s_rt = nullptr;

// ===================================================================
// Status byte bitfield (from PGN 254 steer data)
// ===================================================================
constexpr uint8_t STATUS_BIT_WORK_SWITCH   = 0x01;  // bit 0
constexpr uint8_t STATUS_BIT_STEER_SWITCH  = 0x02;  // bit 1
constexpr uint8_t STATUS_BIT_STEER_ON      = 0x04;  // bit 2
constexpr uint8_t CONFIG_BIT_INVERT_WAS            = 0x01;  // PGN251 set0 bit0
constexpr uint8_t CONFIG_BIT_RELAY_ACTIVE_HIGH     = 0x02;  // PGN251 set0 bit1
constexpr uint8_t CONFIG_BIT_MOTOR_DIR_INVERT      = 0x04;  // PGN251 set0 bit2
constexpr uint8_t CONFIG_BIT_SINGLE_INPUT_WAS      = 0x08;  // PGN251 set0 bit3
constexpr uint8_t CONFIG_BIT_DRIVER_CYTRON         = 0x10;  // PGN251 set0 bit4
constexpr uint8_t CONFIG_BIT_STEER_SWITCH_ENABLE   = 0x20;  // PGN251 set0 bit5
constexpr uint8_t CONFIG_BIT_STEER_BUTTON_ENABLE   = 0x40;  // PGN251 set0 bit6
constexpr uint8_t CONFIG_BIT_SHAFT_ENCODER_ENABLE  = 0x80;  // PGN251 set0 bit7
constexpr int16_t STEER_STATUS_HEADING_INVALID_X10 = 9999;
constexpr int16_t STEER_STATUS_ROLL_INVALID_X10 = 8888;

// ===================================================================
// Send interval tracking
// ===================================================================
static uint32_t s_last_send_ms = 0;
static const uint32_t SEND_INTERVAL_MS = 100;  // 10 Hz

// Rate-limit log messages to avoid serial spam from broadcast echo
static uint32_t s_last_invalid_log_ms = 0;
static uint32_t s_last_unhandled_log_ms = 0;

// RTCM raw-bytestream buffering + telemetry
static constexpr size_t RTCM_RING_CAPACITY = 4096;
static uint8_t s_rtcm_ring[RTCM_RING_CAPACITY];
static size_t s_rtcm_head = 0;
static size_t s_rtcm_tail = 0;
static size_t s_rtcm_size = 0;
static NetRtcmTelemetry s_rtcm_tm;

// ===================================================================
// Network config instance (declared in mod_network.h, was in pgn_types.h)
// ===================================================================
AogNetworkConfig g_net_cfg;

// ===================================================================
// Dependencies: requires at least ETH
// ===================================================================
static const ModuleId s_deps[] = {
    ModuleId::ETH,
    ModuleId::COUNT   // nullptr-terminated sentinel
};

// ===================================================================
// Helper functions (migrated from net.cpp)
// ===================================================================

static int16_t scaleToInt16(float value, float scale) {
    const float scaled = value * scale;
    if (scaled > 32767.0f) return 32767;
    if (scaled < -32768.0f) return -32768;
    return static_cast<int16_t>(scaled);
}

static uint8_t pidOutputToPwmDisplay(uint16_t pid_output, bool settings_received) {
    if (settings_received) {
        return pid_output > 255u ? 255u : static_cast<uint8_t>(pid_output);
    }
    return static_cast<uint8_t>((static_cast<uint32_t>(pid_output) * 255u) / 65535u);
}

static uint8_t mapUm980FixQualityToAog(uint8_t um980_fix_type, bool rtcm_active) {
    uint8_t mapped = 0;
    switch (um980_fix_type) {
        case 1: mapped = 1; break;  // GPS
        case 2: mapped = 2; break;  // DGPS
        case 4: mapped = 4; break;  // RTK fixed
        case 5: mapped = 5; break;  // RTK float
        default: mapped = 0; break;
    }

    if (!rtcm_active) {
        if (mapped == 2 || mapped == 4 || mapped == 5) {
            mapped = 1; // degrade to plain GPS
        }
    }
    return mapped;
}

static int16_t encodeDifferentialAgeX100Ms(uint32_t differential_age_ms, bool rtcm_active) {
    if (!rtcm_active) return 0;
    const uint32_t scaled = differential_age_ms * 100u;
    if (scaled > static_cast<uint32_t>(INT16_MAX)) return INT16_MAX;
    return static_cast<int16_t>(scaled);
}

static uint16_t speedKmhToMmPerSec(float speed_kmh) {
    if (speed_kmh <= 0.0f) return 0;
    const float mm_per_sec = speed_kmh * (1000000.0f / 3600.0f);
    if (mm_per_sec >= 65535.0f) return 65535u;
    return static_cast<uint16_t>(mm_per_sec);
}

static bool startsWithIgnoreCase(const char* text, const char* prefix) {
    if (!text || !prefix) return false;
    while (*prefix) {
        if (*text == '\0') return false;
        if (std::tolower(static_cast<unsigned char>(*text)) !=
            std::tolower(static_cast<unsigned char>(*prefix))) {
            return false;
        }
        ++text;
        ++prefix;
    }
    return true;
}

static void processHardwareMessageCommand(const char* msg_text) {
    if (!msg_text || !*msg_text) return;

    if (startsWithIgnoreCase(msg_text, "diag net")) {
        diagPrintNet();
        return;
    }
    if (startsWithIgnoreCase(msg_text, "diag mem")) {
        diagPrintMem();
        return;
    }
    if (startsWithIgnoreCase(msg_text, "diag hw")) {
        diagPrintHw();
        return;
    }
    if (startsWithIgnoreCase(msg_text, "setup start")) {
        setupWizardRequestStart();
        LOGI(TAG, "HW message command accepted: setup wizard requested");
        return;
    }
}

static void applySteerConfigBits(const AogSteerConfigIn& config) {
    RuntimeConfig& rt_cfg = softConfigGet();
    rt_cfg.actuator_type =
        (config.set0 & CONFIG_BIT_DRIVER_CYTRON) ? 1U : 2U;

    LOGI(TAG,
         "SteerConfig bits: invert_was=%u relay_active_high=%u motor_dir_invert=%u single_was=%u driver=%s steer_switch=%u steer_button=%u shaft_encoder=%u",
         (unsigned)((config.set0 & CONFIG_BIT_INVERT_WAS) != 0),
         (unsigned)((config.set0 & CONFIG_BIT_RELAY_ACTIVE_HIGH) != 0),
         (unsigned)((config.set0 & CONFIG_BIT_MOTOR_DIR_INVERT) != 0),
         (unsigned)((config.set0 & CONFIG_BIT_SINGLE_INPUT_WAS) != 0),
         (config.set0 & CONFIG_BIT_DRIVER_CYTRON) ? "Cytron" : "IBT2",
         (unsigned)((config.set0 & CONFIG_BIT_STEER_SWITCH_ENABLE) != 0),
         (unsigned)((config.set0 & CONFIG_BIT_STEER_BUTTON_ENABLE) != 0),
         (unsigned)((config.set0 & CONFIG_BIT_SHAFT_ENCODER_ENABLE) != 0));
}

// ===================================================================
// RTCM ring buffer (migrated from net.cpp)
// ===================================================================

static size_t rtcmRingWrite(const uint8_t* data, size_t len) {
    if (len == 0) return 0;
    const size_t free_space = RTCM_RING_CAPACITY - s_rtcm_size;
    const size_t to_copy = (len < free_space) ? len : free_space;
    if (to_copy == 0) return 0;

    const size_t first_chunk = ((s_rtcm_head + to_copy) <= RTCM_RING_CAPACITY)
        ? to_copy
        : (RTCM_RING_CAPACITY - s_rtcm_head);
    memcpy(&s_rtcm_ring[s_rtcm_head], data, first_chunk);

    const size_t second_chunk = to_copy - first_chunk;
    if (second_chunk > 0) {
        memcpy(&s_rtcm_ring[0], data + first_chunk, second_chunk);
    }

    s_rtcm_head = (s_rtcm_head + to_copy) % RTCM_RING_CAPACITY;
    s_rtcm_size += to_copy;
    return to_copy;
}

static size_t rtcmRingPeekLinear(const uint8_t** out_ptr) {
    if (s_rtcm_size == 0) {
        *out_ptr = nullptr;
        return 0;
    }

    *out_ptr = &s_rtcm_ring[s_rtcm_tail];
    const size_t linear = RTCM_RING_CAPACITY - s_rtcm_tail;
    return (s_rtcm_size < linear) ? s_rtcm_size : linear;
}

static void rtcmRingPop(size_t len) {
    if (len >= s_rtcm_size) {
        s_rtcm_head = 0;
        s_rtcm_tail = 0;
        s_rtcm_size = 0;
        return;
    }

    s_rtcm_tail = (s_rtcm_tail + len) % RTCM_RING_CAPACITY;
    s_rtcm_size -= len;
}

#if FEAT_GNSS
static void pollRtcmReceiveAndForward(void) {
    uint8_t rtcm_buf[aog_frame::MAX_FRAME];

    while (true) {
        uint16_t src_port = 0;
        const int rx_len = hal_net_receive_rtcm(rtcm_buf, sizeof(rtcm_buf), &src_port);
        if (rx_len <= 0) break;

        s_rtcm_tm.rx_bytes += static_cast<uint32_t>(rx_len);
        s_rtcm_tm.last_activity_ms = hal_millis();

        const size_t written = rtcmRingWrite(rtcm_buf, static_cast<size_t>(rx_len));
        if (written < static_cast<size_t>(rx_len)) {
            s_rtcm_tm.dropped_packets++;
            s_rtcm_tm.overflow_bytes += static_cast<uint32_t>(static_cast<size_t>(rx_len) - written);
            LOGW(TAG, "RTCM ring overflow: dropped %u bytes from port %u (fill=%u/%u)",
                 static_cast<unsigned>(static_cast<size_t>(rx_len) - written),
                 src_port,
                 static_cast<unsigned>(s_rtcm_size),
                 static_cast<unsigned>(RTCM_RING_CAPACITY));
        }
    }

    while (s_rtcm_size > 0) {
        const uint8_t* chunk = nullptr;
        const size_t chunk_len = rtcmRingPeekLinear(&chunk);
        if (!chunk || chunk_len == 0) break;

        const size_t accepted = hal_gnss_rtcm_write(chunk, chunk_len);
        if (accepted == 0) break;
        if (accepted > chunk_len) break;

        rtcmRingPop(accepted);
        s_rtcm_tm.forwarded_bytes += static_cast<uint32_t>(accepted);
        if (accepted < chunk_len) {
            s_rtcm_tm.partial_writes++;
            break;
        }
    }
}
#endif

// ===================================================================
// Module hello/subnet replies (migrated from modules.cpp)
// TODO: Implement full AOG module hello/subnet protocol
// ===================================================================

static void sendModuleHellos(void) {
    // TODO: Send hello replies for all active AgOpenGPS modules (Steer, GPS, Machine).
    // For now, log that we received a Hello from AgIO.
    LOGI(TAG, "AgIO Hello received — module hellos TODO");
}

static void sendModuleSubnetReplies(void) {
    // TODO: Send subnet replies for all active AgOpenGPS modules.
    // For now, log that we received a Scan from AgIO.
    LOGI(TAG, "AgIO Scan received — module subnet replies TODO");
}

void mod_network_send_startup_errors(void) {
    // TODO: Send PGN 0xDD hardware error messages for failed modules.
    // For now, log module health status.
    LOGI(TAG, "startup errors — TODO: send PGN 0xDD for failed modules");
}

// ===================================================================
// Public: update UM980 GNSS status (for PGN 214 encoding)
// ===================================================================

void mod_network_update_um980_status(uint8_t um980_fix_type,
                                      bool rtcm_active,
                                      uint32_t differential_age_ms) {
    const uint32_t now_ms = hal_millis();
    const uint8_t fix_quality = mapUm980FixQualityToAog(um980_fix_type, rtcm_active);
    const int16_t age_x100_ms = encodeDifferentialAgeX100Ms(differential_age_ms, rtcm_active);

    StateLock lock;
    g_nav.gnss.um980_fix_type = um980_fix_type;
    g_nav.gnss.um980_rtcm_active = rtcm_active;
    g_nav.gnss.um980_status_timestamp_ms = now_ms;
    g_nav.gnss.gps_fix_quality = fix_quality;
    g_nav.gnss.gps_diff_age_x100_ms = age_x100_ms;
}

// ===================================================================
// Public: RTCM telemetry
// ===================================================================

void mod_network_get_rtcm_telemetry(NetRtcmTelemetry* out) {
    if (!out) return;
    *out = s_rtcm_tm;
}

// ===================================================================
// Process a single validated frame (migrated from net.cpp)
// ===================================================================

static void processFrame(uint8_t src, uint8_t pgn,
                         const uint8_t* payload, size_t payload_len) {
    // Only process frames from AgIO (0x7F).
    if (src != aog_src::AGIO) return;

    switch (pgn) {
        case aog_pgn::HELLO_FROM_AGIO: {
            AogHelloFromAgio msg;
            if (pgnDecodeHelloFromAgio(payload, payload_len, &msg)) {
                LOGI(TAG, "Hello from AgIO (module=0x%02X, ver=%u)",
                        (unsigned)msg.moduleId, (unsigned)msg.agioVersion);
                mod_network_send_startup_errors();
                sendModuleHellos();
            }
            break;
        }

        case aog_pgn::SCAN_REQUEST: {
            if (pgnDecodeScanRequest(payload, payload_len)) {
                LOGI(TAG, "Scan request from AgIO");
                mod_network_send_startup_errors();
                sendModuleSubnetReplies();
            }
            break;
        }

        case aog_pgn::SUBNET_CHANGE: {
            AogSubnetChange msg;
            if (pgnDecodeSubnetChange(payload, payload_len, &msg)) {
                g_net_cfg.dest_ip[0] = msg.ip_one;
                g_net_cfg.dest_ip[1] = msg.ip_two;
                g_net_cfg.dest_ip[2] = msg.ip_three;
                g_net_cfg.dest_ip[3] = 255;

                hal_net_set_dest_ip(msg.ip_one, msg.ip_two, msg.ip_three, 255);

                LOGI(TAG, "subnet changed, dest=%u.%u.%u.%u",
                        g_net_cfg.dest_ip[0], g_net_cfg.dest_ip[1],
                        g_net_cfg.dest_ip[2], g_net_cfg.dest_ip[3]);
            }
            break;
        }

        case aog_pgn::STEER_DATA_IN: {
            AogSteerDataIn msg;
            if (pgnDecodeSteerDataIn(payload, payload_len, &msg)) {
                const float steer_setpoint_deg = msg.steerAngle / 100.0f;
                const float speed_kmh = msg.speed / 10.0f;
                const uint32_t now_ms = hal_millis();

                {
                    StateLock lock;
                    g_nav.sw.work_switch      = (msg.status & STATUS_BIT_WORK_SWITCH) != 0;
                    g_nav.sw.steer_switch     = (msg.status & STATUS_BIT_STEER_SWITCH) != 0;
                    g_nav.sw.last_status_byte = msg.status;
                    g_nav.sw.gps_speed_kmh     = speed_kmh;
                    g_nav.sw.watchdog_timer_ms = now_ms;
                    g_nav.sw.desiredSteerAngleDeg = steer_setpoint_deg;
                }
            }
            break;
        }

        case aog_pgn::HARDWARE_MESSAGE: {
            uint8_t dur = 0, color = 0;
            char msg_text[128];
            if (pgnDecodeHardwareMessage(payload, payload_len,
                                         &dur, &color,
                                         msg_text, sizeof(msg_text))) {
                LOGI(TAG, "HW message from AgIO: [%u] (col=%u) \"%s\"",
                        (unsigned)dur, (unsigned)color, msg_text);
                processHardwareMessageCommand(msg_text);
            }
            break;
        }

        case aog_pgn::STEER_SETTINGS_IN: {
            AogSteerSettingsIn settings;
            if (pgnDecodeSteerSettingsIn(payload, payload_len, &settings)) {
                // Apply settings to PID controller via mod_steer
                mod_steer_apply_agio_settings(settings.kp, settings.highPWM, settings.lowPWM,
                                              settings.minPWM, settings.countsPerDegree,
                                              settings.wasOffset, settings.ackerman);
                LOGI(TAG, "SteerSettings applied (Kp=%u hiPWM=%u loPWM=%u minPWM=%u cnt=%u off=%d ack=%u)",
                        (unsigned)settings.kp, (unsigned)settings.highPWM,
                        (unsigned)settings.lowPWM, (unsigned)settings.minPWM,
                        (unsigned)settings.countsPerDegree, (int)settings.wasOffset,
                        (unsigned)settings.ackerman);
            }
            break;
        }

        case aog_pgn::STEER_CONFIG_IN: {
            AogSteerConfigIn config;
            if (pgnDecodeSteerConfigIn(payload, payload_len, &config)) {
                LOGI(TAG, "SteerConfig received (set0=0x%02X pulse=%u speed=%u ackFix=%u)",
                        (unsigned)config.set0, (unsigned)config.maxPulse,
                        (unsigned)config.minSpeed, (unsigned)config.ackermanFix);

                {
                    StateLock lock;
                    g_nav.pid.config_set0      = config.set0;
                    g_nav.pid.config_max_pulse = config.maxPulse;
                    g_nav.pid.config_min_speed = config.minSpeed;
                    g_nav.pid.config_received  = true;
                }
                applySteerConfigBits(config);
            }
            break;
        }

        default: {
            uint32_t now = hal_millis();
            if (now - s_last_unhandled_log_ms >= 10000) {
                s_last_unhandled_log_ms = now;
                const char* name = pgnGetName(pgn);
                LOGW(TAG, "unhandled PGN 0x%02X (%s) from Src 0x%02X (len=%zu)",
                        pgn, name, src, payload_len);
            }
            break;
        }
    }
}

// ===================================================================
// Poll for received UDP frames (migrated from net.cpp netPollReceive)
// ===================================================================

static void pollReceive(void) {
#if FEAT_GNSS
    pollRtcmReceiveAndForward();
#endif

    uint8_t rx_buf[aog_frame::MAX_FRAME];

    while (true) {
        uint16_t src_port = 0;
        int rx_len = hal_net_receive(rx_buf, sizeof(rx_buf), &src_port);

        if (rx_len <= 0) break;

        if (rx_buf[0] != AOG_PREAMBLE_1) continue;

        uint8_t frame_src = 0;
        uint8_t frame_pgn = 0;
        const uint8_t* payload = nullptr;
        size_t payload_len = 0;

        if (pgnValidateFrame(rx_buf, static_cast<size_t>(rx_len),
                             &frame_src, &frame_pgn,
                             &payload, &payload_len)) {
            processFrame(frame_src, frame_pgn, payload, payload_len);
        } else {
            uint32_t now = hal_millis();
            if (now - s_last_invalid_log_ms >= 10000) {
                s_last_invalid_log_ms = now;
                LOGW(TAG, "invalid frame (%d bytes from port %u, first=0x%02X)",
                        rx_len, src_port, rx_buf[0]);
            }
        }
    }
}

// ===================================================================
// Send periodic AOG frames (migrated from net.cpp netSendAogFrames)
// ===================================================================

static void sendAogFrames(void) {
    if (!hal_net_is_connected()) return;

    uint32_t now = hal_millis();
    if (now - s_last_send_ms < SEND_INTERVAL_MS) return;
    s_last_send_ms = now;

    struct NetTxSnapshot {
        float steer_angle_deg = 0.0f;
        float heading_deg = 0.0f;
        float roll_deg = 0.0f;
        bool safety_ok = false;
        bool work_switch = false;
        bool steer_switch = false;
        uint16_t pid_output = 0;
        bool settings_received = false;
        uint32_t imu_timestamp_ms = 0;
        bool imu_quality_ok = false;
        uint32_t heading_timestamp_ms = 0;
        bool heading_quality_ok = false;
        float gps_speed_kmh = 0.0f;
        uint8_t gps_fix_quality = 0;
        int16_t gps_diff_age_x100_ms = 0;
    } snap;

    {
        StateLock lock;
        snap.steer_angle_deg = g_nav.steer.steer_angle_deg;
        snap.heading_deg = g_nav.imu.heading_deg;
        snap.roll_deg = g_nav.imu.roll_deg;
        snap.safety_ok = g_nav.safety.safety_ok;
        snap.work_switch = g_nav.sw.work_switch;
        snap.steer_switch = g_nav.sw.steer_switch;
        snap.pid_output = g_nav.pid.pid_output;
        snap.settings_received = g_nav.pid.settings_received;
        snap.imu_timestamp_ms = g_nav.imu.imu_timestamp_ms;
        snap.imu_quality_ok = g_nav.imu.imu_quality_ok;
        snap.heading_timestamp_ms = g_nav.imu.heading_timestamp_ms;
        snap.heading_quality_ok = g_nav.imu.heading_quality_ok;
        snap.gps_speed_kmh = g_nav.sw.gps_speed_kmh;
        snap.gps_fix_quality = g_nav.gnss.gps_fix_quality;
        snap.gps_diff_age_x100_ms = g_nav.gnss.gps_diff_age_x100_ms;
    }

    uint8_t tx_buf[aog_frame::MAX_FRAME];
    size_t tx_len = 0;

    const bool is_paused = opModeIsPausedStatusBit();
    const int16_t angle_x100 = is_paused ? 0 : scaleToInt16(snap.steer_angle_deg, 100.0f);
    const bool imu_valid =
        dep_policy::isImuInputValid(now, snap.imu_timestamp_ms, snap.imu_quality_ok);
    const bool heading_valid =
        dep_policy::isFresh(now, snap.heading_timestamp_ms, dep_policy::IMU_FRESHNESS_TIMEOUT_MS) &&
        snap.heading_quality_ok;
    const int16_t heading_x10 = heading_valid
        ? scaleToInt16(snap.heading_deg, 10.0f)
        : STEER_STATUS_HEADING_INVALID_X10;
    const int16_t roll_x10 = imu_valid
        ? scaleToInt16(snap.roll_deg, 10.0f)
        : STEER_STATUS_ROLL_INVALID_X10;

    uint8_t switch_st = 0;
    if (!snap.safety_ok)  switch_st |= 0x80;
    if (snap.work_switch) switch_st |= 0x01;
    if (snap.steer_switch) switch_st |= 0x02;
    if (is_paused) switch_st |= 0x40;

    const uint8_t pwm_disp = is_paused ? 0 : pidOutputToPwmDisplay(snap.pid_output, snap.settings_received);

    tx_len = pgnEncodeSteerStatusOut(tx_buf, sizeof(tx_buf),
                                     angle_x100, heading_x10, roll_x10,
                                     switch_st, pwm_disp);
    if (tx_len > 0) {
        hal_net_send(tx_buf, tx_len, aog_port::STEER);
    }

    const uint8_t sensor_val = hal_steer_angle_read_sensor_byte();
    tx_len = pgnEncodeFromAutosteer2(tx_buf, sizeof(tx_buf), sensor_val);
    if (tx_len > 0) {
        hal_net_send(tx_buf, tx_len, aog_port::STEER);
    }

    AogGpsMainOut gps = {};
    gps.heading = heading_valid ? scaleToInt16(snap.heading_deg, 16.0f) : 0;
    gps.dualHeading = gps.heading;
    gps.speed = speedKmhToMmPerSec(snap.gps_speed_kmh);
    gps.roll = imu_valid ? scaleToInt16(snap.roll_deg, 16.0f) : 0;
    gps.fixQuality = snap.gps_fix_quality;
    gps.age = snap.gps_diff_age_x100_ms;
    gps.imuHeading = gps.heading;
    gps.imuRoll = gps.roll;

    tx_len = pgnEncodeGpsMainOut(tx_buf, sizeof(tx_buf), gps);
    if (tx_len > 0) {
        hal_net_send(tx_buf, tx_len, aog_port::GPS);
    }
}

// ===================================================================
// Helper: check if at least one transport is healthy
// ===================================================================
static bool anyTransportHealthy(void) {
    if (moduleSysIsActive(ModuleId::ETH) &&
        moduleSysIsHealthy(ModuleId::ETH, hal_millis())) {
        return true;
    }
    if (moduleSysIsActive(ModuleId::WIFI) &&
        moduleSysIsHealthy(ModuleId::WIFI, hal_millis())) {
        return true;
    }
    return false;
}

// ===================================================================
// Lifecycle
// ===================================================================
static bool network_is_enabled(void) {
    return true;
}

static void network_activate(void) {
    s_rt = moduleSysGet(ModuleId::NETWORK);
    if (!s_rt) {
        LOGE(TAG, "moduleSysGet(NETWORK) returned nullptr");
        return;
    }

    s_state.detected = moduleSysIsActive(ModuleId::ETH) ||
                       moduleSysIsActive(ModuleId::WIFI);

    if (s_state.detected) {
        LOGI(TAG, "activated — transport available (ETH=%d WIFI=%d)",
             moduleSysIsActive(ModuleId::ETH),
             moduleSysIsActive(ModuleId::WIFI));
    } else {
        LOGW(TAG, "activated — no transport available");
        s_state.error_code = ERR_NO_TRANSPORT;
    }

    s_state.last_update_ms = hal_millis();
    if (s_rt) s_rt->state = s_state;
}

static void network_deactivate(void) {
    LOGI(TAG, "deactivated");
    s_state.detected = false;
    s_state.quality_ok = false;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
}

static bool network_is_healthy(uint32_t now_ms) {
    if (!s_state.detected) {
        s_state.detected = moduleSysIsActive(ModuleId::ETH) ||
                           moduleSysIsActive(ModuleId::WIFI);
        if (!s_state.detected) {
            s_state.error_code = ERR_NO_TRANSPORT;
            s_state.quality_ok = false;
            if (s_rt) s_rt->state = s_state;
            return false;
        }
    }

    if (!anyTransportHealthy()) {
        s_state.quality_ok = false;
        s_state.error_code = ERR_NO_TRANSPORT;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

    if ((now_ms - s_state.last_update_ms) > FRESHNESS_MS) {
        s_state.quality_ok = false;
        if (s_rt) s_rt->state = s_state;
        return false;
    }

    s_state.quality_ok = true;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
    return true;
}

// ===================================================================
// Pipeline
// ===================================================================
static ModuleResult network_input(uint32_t now_ms) {
    if (!anyTransportHealthy()) {
        return {true, 0};
    }

    pollReceive();

    s_state.last_update_ms = now_ms;
    s_state.quality_ok = true;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
    return MOD_OK;
}

static ModuleResult network_process(uint32_t /*now_ms*/) {
    return MOD_OK;
}

static ModuleResult network_output(uint32_t now_ms) {
    if (!anyTransportHealthy()) {
        return {true, 0};
    }

    sendAogFrames();

    s_state.last_update_ms = now_ms;
    s_state.quality_ok = true;
    s_state.error_code = 0;
    if (s_rt) s_rt->state = s_state;
    return MOD_OK;
}

// ===================================================================
// Configuration (no user-configurable NVS keys)
// ===================================================================
static bool network_cfg_get(const char* key, char* buf, size_t len) {
    (void)key; (void)buf; (void)len;
    return false;
}

static bool network_cfg_set(const char* key, const char* val) {
    (void)key; (void)val;
    return false;
}

static bool network_cfg_apply(void) {
    return true;
}

static bool network_cfg_save(void) {
    return true;
}

static bool network_cfg_load(void) {
    return true;
}

static bool network_cfg_show(void) {
    LOGI(TAG, "config: (no user-configurable keys — PGN config from AgIO)");
    LOGI(TAG, "  detected=%d quality_ok=%d error=%ld",
         s_state.detected, s_state.quality_ok, (long)s_state.error_code);
    LOGI(TAG, "  ETH active=%d healthy=%d",
         moduleSysIsActive(ModuleId::ETH),
         moduleSysIsHealthy(ModuleId::ETH, hal_millis()));
    LOGI(TAG, "  WIFI active=%d healthy=%d",
         moduleSysIsActive(ModuleId::WIFI),
         moduleSysIsHealthy(ModuleId::WIFI, hal_millis()));
    return true;
}

// ===================================================================
// Debug
// ===================================================================
static bool network_debug(void) {
    LOGI(TAG, "=== NETWORK Debug ===");
    network_cfg_show();
    return true;
}

// ===================================================================
// Ops table
// ===================================================================
const ModuleOps2 mod_network_ops = {
    .name      = "NETWORK",
    .id        = ModuleId::NETWORK,

    .is_enabled  = network_is_enabled,
    .activate    = network_activate,
    .deactivate  = network_deactivate,
    .is_healthy  = network_is_healthy,

    .input   = network_input,
    .process = network_process,
    .output  = network_output,

    .cfg_keys   = nullptr,
    .cfg_get   = network_cfg_get,
    .cfg_set   = network_cfg_set,
    .cfg_apply = network_cfg_apply,
    .cfg_save  = network_cfg_save,
    .cfg_load  = network_cfg_load,
    .cfg_show  = network_cfg_show,

    .debug = network_debug,

    .deps = s_deps,
};
