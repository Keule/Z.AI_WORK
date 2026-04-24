/**
 * @file nvs_config.cpp
 * @brief NVS persistence helpers for RuntimeConfig — Phase 0 (S0-05).
 */

#include "nvs_config.h"
#include "hal/hal.h"

#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <nvs.h>
#endif

namespace {
static constexpr const char* NTRIP_MOUNT_LEGACY_KEY = "ntrip_mountpoint";

static uint32_t floatToU32(float value) {
    uint32_t out = 0;
    std::memcpy(&out, &value, sizeof(out));
    return out;
}

static float u32ToFloat(uint32_t value) {
    float out = 0.0f;
    std::memcpy(&out, &value, sizeof(out));
    return out;
}

#if defined(ARDUINO_ARCH_ESP32)
static void loadString(nvs_handle_t handle, const char* key, char* dst, size_t dst_size) {
    size_t len = dst_size;
    if (nvs_get_str(handle, key, dst, &len) == ESP_OK) {
        dst[dst_size - 1] = '\0';
    }
}
#endif

}  // namespace

void nvsConfigLoad(RuntimeConfig& cfg) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    loadString(handle, nvs_keys::NTRIP_HOST, cfg.ntrip_host, sizeof(cfg.ntrip_host));
    loadString(handle, nvs_keys::NTRIP_MOUNT, cfg.ntrip_mountpoint, sizeof(cfg.ntrip_mountpoint));
    if (cfg.ntrip_mountpoint[0] == '\0') {
        loadString(handle, NTRIP_MOUNT_LEGACY_KEY, cfg.ntrip_mountpoint, sizeof(cfg.ntrip_mountpoint));
    }
    loadString(handle, nvs_keys::NTRIP_USER, cfg.ntrip_user, sizeof(cfg.ntrip_user));
    loadString(handle, nvs_keys::NTRIP_PASS, cfg.ntrip_password, sizeof(cfg.ntrip_password));

    uint16_t ntrip_port = 0;
    if (nvs_get_u16(handle, nvs_keys::NTRIP_PORT, &ntrip_port) == ESP_OK) {
        cfg.ntrip_port = ntrip_port;
    }

    uint32_t u32 = 0;
    if (nvs_get_u32(handle, nvs_keys::PID_KP, &u32) == ESP_OK) cfg.pid_kp = u32ToFloat(u32);
    if (nvs_get_u32(handle, nvs_keys::PID_KI, &u32) == ESP_OK) cfg.pid_ki = u32ToFloat(u32);
    if (nvs_get_u32(handle, nvs_keys::PID_KD, &u32) == ESP_OK) cfg.pid_kd = u32ToFloat(u32);

    uint8_t u8 = 0;
    if (nvs_get_u8(handle, nvs_keys::NET_MODE, &u8) == ESP_OK) cfg.net_mode = u8;
    if (nvs_get_u32(handle, nvs_keys::NET_IP, &u32) == ESP_OK) cfg.net_ip = u32;
    if (nvs_get_u32(handle, nvs_keys::NET_GW, &u32) == ESP_OK) cfg.net_gateway = u32;
    if (nvs_get_u32(handle, nvs_keys::NET_SUBNET, &u32) == ESP_OK) cfg.net_subnet = u32;
    if (nvs_get_u8(handle, nvs_keys::ACT_TYPE, &u8) == ESP_OK) cfg.actuator_type = u8;

    // NTRIP reconnect
    if (nvs_get_u32(handle, nvs_keys::NTRIP_RECONNECT, &u32) == ESP_OK) {
        if (u32 >= 1000 && u32 <= 60000) cfg.ntrip_reconnect_ms = u32;
    }

    // GNSS
    if (nvs_get_u32(handle, nvs_keys::GNSS_BAUD, &u32) == ESP_OK) cfg.gnss_baud = u32;
    if (nvs_get_u32(handle, nvs_keys::GNSS_UART_A_BAUD, &u32) == ESP_OK) cfg.gnss_uart_a_baud = u32;
    if (nvs_get_u32(handle, nvs_keys::GNSS_UART_B_BAUD, &u32) == ESP_OK) cfg.gnss_uart_b_baud = u32;
    if (nvs_get_u8(handle, nvs_keys::GNSS_UART_A_ROLE, &u8) == ESP_OK) cfg.gnss_uart_a_role = u8;
    if (nvs_get_u8(handle, nvs_keys::GNSS_UART_B_ROLE, &u8) == ESP_OK) cfg.gnss_uart_b_role = u8;

    nvs_close(handle);
#else
    (void)cfg;
#endif
}

bool nvsConfigSave(const RuntimeConfig& cfg) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    const esp_err_t open_err = nvs_open(nvs_keys::NS, NVS_READWRITE, &handle);
    if (open_err != ESP_OK) {
        hal_log("NVS: open(%s) failed: %s", nvs_keys::NS, esp_err_to_name(open_err));
        return false;
    }

    bool ok = true;
    auto check_set = [&](esp_err_t err, const char* key) {
        if (err != ESP_OK) {
            ok = false;
            hal_log("NVS: set key '%s' failed: %s", key, esp_err_to_name(err));
        }
    };

    check_set(nvs_set_str(handle, nvs_keys::NTRIP_HOST, cfg.ntrip_host), nvs_keys::NTRIP_HOST);
    check_set(nvs_set_u16(handle, nvs_keys::NTRIP_PORT, cfg.ntrip_port), nvs_keys::NTRIP_PORT);
    check_set(nvs_set_str(handle, nvs_keys::NTRIP_MOUNT, cfg.ntrip_mountpoint), nvs_keys::NTRIP_MOUNT);
    check_set(nvs_set_str(handle, nvs_keys::NTRIP_USER, cfg.ntrip_user), nvs_keys::NTRIP_USER);
    check_set(nvs_set_str(handle, nvs_keys::NTRIP_PASS, cfg.ntrip_password), nvs_keys::NTRIP_PASS);

    check_set(nvs_set_u32(handle, nvs_keys::PID_KP, floatToU32(cfg.pid_kp)), nvs_keys::PID_KP);
    check_set(nvs_set_u32(handle, nvs_keys::PID_KI, floatToU32(cfg.pid_ki)), nvs_keys::PID_KI);
    check_set(nvs_set_u32(handle, nvs_keys::PID_KD, floatToU32(cfg.pid_kd)), nvs_keys::PID_KD);

    check_set(nvs_set_u8(handle, nvs_keys::NET_MODE, cfg.net_mode), nvs_keys::NET_MODE);
    check_set(nvs_set_u32(handle, nvs_keys::NET_IP, cfg.net_ip), nvs_keys::NET_IP);
    check_set(nvs_set_u32(handle, nvs_keys::NET_GW, cfg.net_gateway), nvs_keys::NET_GW);
    check_set(nvs_set_u32(handle, nvs_keys::NET_SUBNET, cfg.net_subnet), nvs_keys::NET_SUBNET);
    check_set(nvs_set_u8(handle, nvs_keys::ACT_TYPE, cfg.actuator_type), nvs_keys::ACT_TYPE);

    // NTRIP reconnect
    check_set(nvs_set_u32(handle, nvs_keys::NTRIP_RECONNECT, cfg.ntrip_reconnect_ms), nvs_keys::NTRIP_RECONNECT);

    // GNSS
    check_set(nvs_set_u32(handle, nvs_keys::GNSS_BAUD, cfg.gnss_baud), nvs_keys::GNSS_BAUD);
    check_set(nvs_set_u32(handle, nvs_keys::GNSS_UART_A_BAUD, cfg.gnss_uart_a_baud), nvs_keys::GNSS_UART_A_BAUD);
    check_set(nvs_set_u32(handle, nvs_keys::GNSS_UART_B_BAUD, cfg.gnss_uart_b_baud), nvs_keys::GNSS_UART_B_BAUD);
    check_set(nvs_set_u8(handle, nvs_keys::GNSS_UART_A_ROLE, cfg.gnss_uart_a_role), nvs_keys::GNSS_UART_A_ROLE);
    check_set(nvs_set_u8(handle, nvs_keys::GNSS_UART_B_ROLE, cfg.gnss_uart_b_role), nvs_keys::GNSS_UART_B_ROLE);

    const esp_err_t commit_err = nvs_commit(handle);
    if (commit_err != ESP_OK) {
        hal_log("NVS: commit failed: %s", esp_err_to_name(commit_err));
    }
    nvs_close(handle);
    return ok && (commit_err == ESP_OK);
#else
    (void)cfg;
    return false;
#endif
}

void nvsConfigFactoryReset(void) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_keys::NS, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    (void)nvs_erase_all(handle);
    (void)nvs_commit(handle);
    nvs_close(handle);
#endif
}

bool nvsConfigHasData(void) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    if (nvs_open(nvs_keys::NS, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t len = 0;
    const bool has_data = (nvs_get_str(handle, nvs_keys::NTRIP_HOST, nullptr, &len) == ESP_OK);
    nvs_close(handle);
    return has_data;
#else
    return false;
#endif
}
