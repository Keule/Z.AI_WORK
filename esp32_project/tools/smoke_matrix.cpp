#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "logic/features.h"
#include "logic/pgn_codec.h"
#include "logic/pgn_types.h"
#include "logic/global_state.h"
#include "hal/hal.h"

// log_ext.h externs (used by inline filter helper)
uint16_t log_filter_line = 0;
char log_filter_file[64] = "";

// global_state.h externs
NavigationState g_nav = {};
volatile float desiredSteerAngleDeg = 0.0f;

extern "C" {
uint32_t hal_millis(void) { return 0; }
uint32_t hal_micros(void) { return 0; }
void hal_delay_ms(uint32_t) {}
void hal_log(const char*, ...) {}
void hal_mutex_init(void) {}
void hal_mutex_lock(void) {}
void hal_mutex_unlock(void) {}
bool hal_safety_ok(void) { return true; }
void hal_sensor_spi_init(void) {}
void hal_sensor_spi_deinit(void) {}
void hal_sensor_spi_reinit(void) {}
void hal_sensor_spi_get_telemetry(HalSpiTelemetry*) {}
void hal_imu_begin(void) {}
bool hal_imu_read(float*, float*, float*) { return false; }
bool hal_imu_detect(void) { return false; }
void hal_steer_angle_begin(void) {}
bool hal_steer_angle_detect(void) { return false; }
void hal_steer_angle_calibrate(void) {}
bool hal_steer_angle_is_calibrated(void) { return false; }
float hal_steer_angle_read_deg(void) { return 0.0f; }
int16_t hal_steer_angle_read_raw(void) { return 0; }
uint8_t hal_steer_angle_read_sensor_byte(void) { return 0; }
void hal_actuator_begin(void) {}
void hal_actuator_write(uint16_t) {}
bool hal_actuator_detect(void) { return false; }
void hal_net_init(void) {}
void hal_net_set_dest_ip(uint8_t, uint8_t, uint8_t, uint8_t) {}
void hal_net_send(const uint8_t*, size_t, uint16_t) {}
int hal_net_receive(uint8_t*, size_t, uint16_t*) { return 0; }
bool hal_net_is_connected(void) { return true; }
bool hal_net_detected(void) { return true; }
}

static bool expect(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "[FAIL] %s\n", msg);
        return false;
    }
    return true;
}

struct TimingStats {
    uint32_t target_us = 0;
    uint32_t samples = 0;
    uint32_t deadline_miss = 0;
    int32_t jitter_peak_us = 0;
};

static TimingStats calcTimingStats(const std::vector<uint32_t>& timestamps_us, uint32_t target_period_us) {
    TimingStats stats{};
    stats.target_us = target_period_us;
    if (timestamps_us.size() < 2) return stats;

    for (size_t i = 1; i < timestamps_us.size(); i++) {
        int32_t dt = static_cast<int32_t>(timestamps_us[i] - timestamps_us[i - 1]);
        int32_t jitter = dt - static_cast<int32_t>(target_period_us);
        stats.jitter_peak_us = std::max(stats.jitter_peak_us, std::abs(jitter));
        if (dt > static_cast<int32_t>(target_period_us)) {
            stats.deadline_miss++;
        }
        stats.samples++;
    }
    return stats;
}

static bool smokeDiscoveryHelloSubnet() {
    bool ok = true;
    // Minimal checksum sanity (independent of the runtime self-test helper).
    uint8_t base_payload[8] = {0};
    uint8_t base_frame[64] = {0};
    size_t base_len = pgnBuildFrame(base_frame, sizeof(base_frame), aog_src::STEER,
                                    aog_pgn::STEER_STATUS_OUT, base_payload, sizeof(base_payload));
    ok &= expect(base_len == 14, "baseline frame size mismatch");
    uint8_t src = 0;
    uint8_t pgn = 0;
    const uint8_t* payload = nullptr;
    size_t payload_len = 0;
    ok &= expect(pgnValidateFrame(base_frame, base_len, &src, &pgn, &payload, &payload_len),
                 "baseline frame validation failed");

    src = 0;
    pgn = 0;
    payload = nullptr;
    payload_len = 0;

    // Discovery (PGN 200)
    const uint8_t hello_from_agio[] = {0x80, 0x81, 0x7F, 0xC8, 0x03, 56, 0x00, 0x00, 0x47};
    ok &= expect(pgnValidateFrame(hello_from_agio, sizeof(hello_from_agio), &src, &pgn, &payload, &payload_len),
                 "PGN 200 frame invalid");
    AogHelloFromAgio hello{};
    ok &= expect(pgnDecodeHelloFromAgio(payload, payload_len, &hello), "PGN 200 decode failed");

    // Scan (PGN 202)
    const uint8_t scan_req[] = {0x80, 0x81, 0x7F, 0xCA, 0x03, 0xCA, 0xCA, 0x05, 0x00};
    ok &= expect(pgnValidateFrame(scan_req, sizeof(scan_req), &src, &pgn, &payload, &payload_len),
                 "PGN 202 frame invalid");
    ok &= expect(pgnDecodeScanRequest(payload, payload_len), "PGN 202 decode failed");

    // Subnet change (PGN 201)
    AogSubnetChange subnet_change{0xC9, 0xC9, 10, 1, 99};
    std::array<uint8_t, 32> frame{};
    size_t frame_len = pgnBuildFrame(frame.data(), frame.size(), aog_src::AGIO, aog_pgn::SUBNET_CHANGE,
                                     &subnet_change, sizeof(subnet_change));
    ok &= expect(frame_len > 0, "PGN 201 build failed");
    ok &= expect(pgnValidateFrame(frame.data(), frame_len, &src, &pgn, &payload, &payload_len),
                 "PGN 201 frame invalid");
    AogSubnetChange decoded{};
    ok &= expect(pgnDecodeSubnetChange(payload, payload_len, &decoded), "PGN 201 decode failed");
    ok &= expect(decoded.ip_one == 10 && decoded.ip_two == 1 && decoded.ip_three == 99,
                 "PGN 201 payload mismatch");

    return ok;
}

static bool smokePgnIoPerProfile() {
    bool ok = true;

    constexpr bool eth = FEAT_COMPILED_ETH;
    constexpr bool ads = FEAT_COMPILED_ADS;
    constexpr bool act = FEAT_COMPILED_ACT;
    constexpr bool safety = FEAT_COMPILED_SAFETY;

    ok &= expect(eth, "FEAT_ETH must be active");

    // outbound examples (always buildable)
    std::array<uint8_t, aog_frame::MAX_FRAME> tx{};
    uint8_t subnet[3] = {255, 255, 255};
    uint8_t ip[4] = {192, 168, 1, 70};
    ok &= expect(pgnEncodeSubnetReply(tx.data(), tx.size(), aog_src::STEER, ip, subnet) > 0,
                 "PGN 203 encode failed");

    if (ads) {
        ok &= expect(pgnEncodeHelloReplySteer(tx.data(), tx.size(), 123, 456, 0x03) > 0,
                     "Steer hello encode failed");
        ok &= expect(pgnEncodeHelloReplyGps(tx.data(), tx.size()) > 0,
                     "GPS hello encode failed");
    }

    if (act || safety) {
        ok &= expect(pgnEncodeFromAutosteer2(tx.data(), tx.size(), 0xD2) > 0,
                     "PGN 250 encode failed");
    }

    // inbound examples relevant for control-capable profiles
    if (act) {
        AogSteerDataIn steer_in{};
        steer_in.status = 0x03;
        steer_in.speed = 42;
        ok &= expect(pgnDecodeSteerDataIn(reinterpret_cast<const uint8_t*>(&steer_in), sizeof(steer_in), &steer_in),
                     "PGN 254 decode failed");
    }

    return ok;
}

static bool smokeTimingMetrics() {
    bool ok = true;

    // Simulated 200 Hz loop with jitter + one severe overrun.
    const std::vector<uint32_t> timestamps_us = {
        0, 5000, 10020, 14970, 20010, 25080, 31080, 36050
    };
    TimingStats stats = calcTimingStats(timestamps_us, 5000);

    ok &= expect(stats.samples == (timestamps_us.size() - 1), "timing sample count mismatch");
    ok &= expect(stats.jitter_peak_us >= 80, "jitter peak metric too small");
    ok &= expect(stats.deadline_miss >= 4, "deadline miss counter too small");

    std::printf("timing: target=%uus samples=%u jitter_peak=%dus deadline_miss=%u\n",
                stats.target_us,
                stats.samples,
                stats.jitter_peak_us,
                stats.deadline_miss);
    return ok;
}

int main() {
    bool ok = true;
    ok &= smokeDiscoveryHelloSubnet();
    ok &= smokePgnIoPerProfile();
    ok &= smokeTimingMetrics();

    if (!ok) {
        std::fprintf(stderr, "smoke matrix failed\n");
        return 1;
    }

    std::puts("smoke matrix passed");
    return 0;
}
