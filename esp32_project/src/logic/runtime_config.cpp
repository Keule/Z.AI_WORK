/**
 * @file runtime_config.cpp
 * @brief RuntimeConfig implementation — TASK-028, TASK-033.
 *
 * Provides the global RuntimeConfig instance and functions to
 * load compile-time defaults and runtime overrides.
 *
 * TASK-033: softConfigLoadOverrides() reads NTRIP credentials from
 * /ntrip.cfg on the SD card.  File format is simple key=value INI:
 *   host=<hostname>
 *   port=<port>
 *   mountpoint=<mountpoint>
 *   user=<username>
 *   password=<password>
 */

#include "runtime_config.h"
#include "soft_config.h"
#include "modules.h"

#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "fw_config.h"   // for SD_SPI_BUS, SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS
#include "hal/hal.h"     // hal_sensor_spi_deinit(), hal_sensor_spi_reinit(), hal_delay_ms()
#endif

/// Path to NTRIP credentials file on SD card.
static constexpr const char* kNtripCfgPath = "/ntrip.cfg";

/// Global runtime configuration instance (zero-initialised).
static RuntimeConfig s_runtime_config = {};

static uint32_t ip4ToU32(const uint8_t ip[4]) {
    return (static_cast<uint32_t>(ip[0]) << 24) |
           (static_cast<uint32_t>(ip[1]) << 16) |
           (static_cast<uint32_t>(ip[2]) << 8) |
           static_cast<uint32_t>(ip[3]);
}

void softConfigLoadDefaults(RuntimeConfig& cfg) {
    // NTRIP
    std::strncpy(cfg.ntrip_host, cfg::NTRIP_HOST, sizeof(cfg.ntrip_host) - 1);
    cfg.ntrip_host[sizeof(cfg.ntrip_host) - 1] = '\0';
    cfg.ntrip_port = cfg::NTRIP_PORT;
    std::strncpy(cfg.ntrip_mountpoint, cfg::NTRIP_MOUNTPOINT, sizeof(cfg.ntrip_mountpoint) - 1);
    cfg.ntrip_mountpoint[sizeof(cfg.ntrip_mountpoint) - 1] = '\0';
    std::strncpy(cfg.ntrip_user, cfg::NTRIP_USER, sizeof(cfg.ntrip_user) - 1);
    cfg.ntrip_user[sizeof(cfg.ntrip_user) - 1] = '\0';
    std::strncpy(cfg.ntrip_password, cfg::NTRIP_PASSWORD, sizeof(cfg.ntrip_password) - 1);
    cfg.ntrip_password[sizeof(cfg.ntrip_password) - 1] = '\0';
    cfg.ntrip_reconnect_ms = cfg::NTRIP_RECONNECT_MS;
    cfg.pid_kp = 1.0f;
    cfg.pid_ki = 0.0f;
    cfg.pid_kd = 0.01f;
    cfg.net_mode = cfg::NET_DHCP ? 0U : 1U;
    cfg.net_ip = ip4ToU32(cfg::NET_IP);
    cfg.net_gateway = ip4ToU32(cfg::NET_GATEWAY);
    cfg.net_subnet = ip4ToU32(cfg::NET_SUBNET);
    cfg.actuator_type = 0U;

    // GNSS
    cfg.gnss_baud = cfg::GNSS_BAUD;

    // Logging
    cfg.log_interval_ms = cfg::LOG_INTERVAL_MS;
    cfg.log_default_active = cfg::LOG_DEFAULT_ACTIVE;
}

#if defined(ARDUINO_ARCH_ESP32)

// Parse a "key=value" line.  Returns pointer past '=' or nullptr if not a match.
static const char* matchKey(const char* line, const char* key) {
    // Skip leading whitespace
    while (*line == ' ' || *line == '\t') line++;
    const char* p = line;
    while (*key && *p && *p != '=' && *p != '\n' && *p != '\r') {
        if (*key++ != *p++) return nullptr;
    }
    if (*key != '\0') return nullptr;
    // Skip whitespace before '='
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=') return nullptr;
    return p + 1;  // past '='
}

// Copy value from line into dst, trimming whitespace and newline.
static void copyValue(char* dst, size_t dst_size, const char* value) {
    if (!value || !*value) { dst[0] = '\0'; return; }
    // Skip whitespace after '='
    while (*value == ' ' || *value == '\t') value++;
    size_t i = 0;
    while (i < dst_size - 1 && *value && *value != '\n' && *value != '\r') {
        dst[i++] = *value++;
    }
    dst[i] = '\0';
    // Trim trailing whitespace
    while (i > 0 && (dst[i - 1] == ' ' || dst[i - 1] == '\t')) {
        dst[--i] = '\0';
    }
}

/// Read NTRIP credentials from SD card file.
/// Returns true if file was found and at least host was read, false otherwise.
static bool loadNtripFromSd(RuntimeConfig& cfg) {
    // On ESP32-S3, SD_SPI_BUS is shared with sensors; deinit/reinit around SD access is mandatory.
    hal_sensor_spi_deinit();
    hal_delay_ms(10);  // brief settle time, same pattern as OTA/SD logger paths

    // Initialise SD card SPI
    SPIClass sdSPI(SD_SPI_BUS);
    sdSPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS);

    bool result = false;
    bool got_host = false;
    File f;

    if (!SD.begin(SD_CS, sdSPI, 4000000, "/sd", 5)) {
        goto cleanup;
    }

    f = SD.open(kNtripCfgPath);
    if (!f) {
        goto cleanup;
    }

    char line[128];

    while (f.available()) {
        int len = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';

        // Skip comments and empty lines
        const char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\0' || *p == '\r' || *p == '\n') continue;

        const char* val;

        if ((val = matchKey(line, "host"))) {
            copyValue(cfg.ntrip_host, sizeof(cfg.ntrip_host), val);
            got_host = true;
        } else if ((val = matchKey(line, "port"))) {
            int port = atoi(val);
            if (port > 0 && port <= 65535) cfg.ntrip_port = (uint16_t)port;
        } else if ((val = matchKey(line, "mountpoint"))) {
            copyValue(cfg.ntrip_mountpoint, sizeof(cfg.ntrip_mountpoint), val);
        } else if ((val = matchKey(line, "user"))) {
            copyValue(cfg.ntrip_user, sizeof(cfg.ntrip_user), val);
        } else if ((val = matchKey(line, "password"))) {
            copyValue(cfg.ntrip_password, sizeof(cfg.ntrip_password), val);
        }
    }

    result = got_host;

cleanup:
    if (f) {
        f.close();
    }
    SD.end();
    sdSPI.end();
    hal_sensor_spi_reinit();
    return result;
}

#endif  // ARDUINO_ARCH_ESP32

bool softConfigLoadOverrides(RuntimeConfig& cfg) {
#if defined(ARDUINO_ARCH_ESP32)
    if (!moduleIsActive(MOD_SD)) {
        hal_log("CFG: MOD_SD inactive -> skip SD override load");
        return false;
    }
    if (loadNtripFromSd(cfg)) {
        return true;
    }
    // No credentials file or SD not present — NTRIP stays with empty defaults
    // (host is ""), so NTRIP will remain in IDLE state.  This is expected.
#endif
    (void)cfg;
    return false;
}

RuntimeConfig& softConfigGet(void) {
    return s_runtime_config;
}
