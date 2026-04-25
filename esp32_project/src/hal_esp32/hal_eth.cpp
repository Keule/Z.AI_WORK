/**
 * @file hal_eth.cpp
 * @brief W5500 Ethernet via ESP-IDF ETH driver (SPI3_HOST).
 *
 * Domain: Network / Ethernet
 * Split from hal_impl.cpp (ADR-HAL-002).
 *
 * Uses ETH.begin() to initialise the W5500 on SPI3_HOST with the
 * pins defined by the LilyGO board design.  The ETH driver handles
 * SPI communication internally - no manual SPI setup needed.
 *
 * Link status and IP assignment are tracked via WiFi.onEvent()
 * callbacks (ARDUINO_EVENT_ETH_CONNECTED / ARDUINO_EVENT_ETH_GOT_IP).
 *
 * UDP sockets for AgIO communication:
 *   - ethUDP_recv: Listen on port 8888 (receives from AgIO)
 *   - ethUDP_send: Send from port 5126 to AgIO port 9999
 *   - ethUDP_rtcm: Listen on RTCM port (raw correction bytes)
 */

#include "hal/hal.h"
#include "fw_config.h"
#include "logic/features.h"
#include "logic/pgn_types.h"
#include "logic/log_config.h"
#include "logic/log_ext.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_HAL
#include "esp_log.h"

#include <Arduino.h>
#include <cstring>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_wifi.h"

// ===================================================================
// ETH driver selection based on Arduino ESP32 Core version
// ===================================================================
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    #include <ETH.h>
#else
    #include "ETHClass2.h"
    #define ETH ETH2
#endif

// ===================================================================
// UDP sockets for AgIO communication
// ===================================================================
static WiFiUDP ethUDP_recv;  // Listen socket – bound to port 8888
static WiFiUDP ethUDP_send;  // Send socket – sends FROM port 5126
static WiFiUDP ethUDP_rtcm;  // Listen socket – RTCM port

// Static IP configuration (default = 0.0.0.0 → DHCP)
static IPAddress s_local_ip(0, 0, 0, 0);
static IPAddress s_subnet(0, 0, 0, 0);
static IPAddress s_gateway(0, 0, 0, 0);
static IPAddress s_dns(8, 8, 8, 8);
static IPAddress s_dest_ip(192, 168, 1, 255);

// Ethernet state tracking
static bool s_w5500_detected = false;
static bool s_eth_link_up    = false;
static bool s_eth_has_ip     = false;

// ===================================================================
// IP address helpers
// ===================================================================
static uint32_t ipToU32(const IPAddress& ip) {
    return (static_cast<uint32_t>(ip[0]) << 24) |
           (static_cast<uint32_t>(ip[1]) << 16) |
           (static_cast<uint32_t>(ip[2]) << 8) |
           static_cast<uint32_t>(ip[3]);
}

static IPAddress u32ToIp(uint32_t value) {
    return IPAddress((value >> 24) & 0xFF,
                     (value >> 16) & 0xFF,
                     (value >> 8) & 0xFF,
                     value & 0xFF);
}

// ===================================================================
// WiFi event handler for Ethernet events
// ===================================================================
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
static void onEthEvent(WiFiEvent_t event, arduino_event_info_t info) {
#else
static void onEthEvent(WiFiEvent_t event) {
#endif
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        hal_log("ETH: driver started");
        ETH.setHostname("agsteer");
        break;

    case ARDUINO_EVENT_ETH_CONNECTED:
        hal_log("ETH: link UP (%s %u Mbps)",
                ETH.fullDuplex() ? "full-duplex" : "half-duplex",
                ETH.linkSpeed());
        s_eth_link_up = true;
        break;

    case ARDUINO_EVENT_ETH_GOT_IP:
        hal_log("ETH: got IP %s  (MAC: %s)",
                ETH.localIP().toString().c_str(),
                ETH.macAddress().c_str());
        s_eth_has_ip = true;

        ethUDP_recv.begin(aog_port::AGIO_LISTEN);
        hal_log("ETH: UDP listening on port %u (AgIO sends here)", aog_port::AGIO_LISTEN);

        ethUDP_rtcm.begin(aog_port::RTCM_LISTEN);
        hal_log("ETH: UDP listening on RTCM port %u (AgIO/NTRIP correction input)",
                aog_port::RTCM_LISTEN);

        ethUDP_send.begin(aog_port::STEER);
        hal_log("ETH: UDP sending from port %u (to AgIO port %u)", aog_port::STEER, aog_port::AGIO_SEND);

        // WiFi radio deaktivieren — Ethernet ist primaere Verbindung,
        // spart CPU/RAM und verhindert WiFi socket errors (errno 113)
        WiFi.mode(WIFI_OFF);
        hal_log("ETH: WiFi radio disabled (Ethernet is primary)");
        break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
        hal_log("ETH: link DOWN");
        s_eth_link_up = false;
        s_eth_has_ip = false;
        ethUDP_recv.stop();
        ethUDP_send.stop();
        ethUDP_rtcm.stop();
        break;

    case ARDUINO_EVENT_ETH_STOP:
        hal_log("ETH: driver stopped");
        s_eth_link_up = false;
        s_eth_has_ip = false;
        ethUDP_recv.stop();
        ethUDP_send.stop();
        ethUDP_rtcm.stop();
        break;

    default:
        break;
    }
}

// ===================================================================
// Public API — Network (hal.h)
// ===================================================================

void hal_net_init(void) {
    WiFi.onEvent(onEthEvent);

    #if CONFIG_IDF_TARGET_ESP32
        hal_log("ETH: initialising RTL8201 ETH  (MDC=%d MDIO=%d RST=%d PWR=%d)...",
            ETH_MDC_PIN, ETH_MDIO_PIN, ETH_RESET_PIN, ETH_POWER_PIN);
        pinMode(ETH_POWER_PIN, OUTPUT);
        digitalWrite(ETH_POWER_PIN, HIGH);
        bool init_ok = ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_RESET_PIN, ETH_CLK_MODE);
    #else
        hal_log("ETH: initialising W5500 ETH on SPI3_HOST (SCK=%d MISO=%d MOSI=%d CS=%d INT=%d RST=%d)...",
            ETH_SCK, ETH_MISO, ETH_MOSI, ETH_CS, ETH_INT, ETH_RST);
        bool init_ok = ETH.begin(
        ETH_PHY_W5500, 1, ETH_CS, ETH_INT, ETH_RST, SPI3_HOST,
        ETH_SCK, ETH_MISO, ETH_MOSI);
    #endif

    if (!init_ok) {
        hal_log("ETH: FAILED - Check Configuration.");
        s_w5500_detected = false;
        return;
    }

    s_w5500_detected = true;
    hal_log("ETH: chip detected OK");

    // Apply IP config: static or DHCP
    if (s_local_ip != IPAddress(0, 0, 0, 0)) {
        if (!ETH.config(s_local_ip, s_gateway, s_subnet, s_dns, s_dns)) {
            hal_log("ETH: static IP config failed");
        }
        hal_log("ETH: static mode %s", s_local_ip.toString().c_str());
    } else {
        hal_log("ETH: DHCP mode");
    }

    uint32_t wait_start = millis();
    while (!s_eth_has_ip && (millis() - wait_start < 5000)) {
        delay(100);
        yield();
    }

    if (s_eth_has_ip) {
        hal_log("ETH: ready - IP=%s", ETH.localIP().toString().c_str());
    } else if (s_eth_link_up) {
        hal_log("ETH: link up but no IP yet (waiting for DHCP...)");
    } else {
        hal_log("ETH: WARNING - no link detected (cable unplugged?)");
    }
}

void hal_net_send(const uint8_t* data, size_t len, uint16_t port) {
    if (!s_eth_has_ip) return;
    ethUDP_send.beginPacket(s_dest_ip, aog_port::AGIO_SEND);
    ethUDP_send.write(data, static_cast<size_t>(len));
    ethUDP_send.endPacket();
}

void hal_net_set_dest_ip(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3) {
    s_dest_ip = IPAddress(ip0, ip1, ip2, ip3);
    hal_log("HAL: dest IP updated to %u.%u.%u.%u", ip0, ip1, ip2, ip3);
}

int hal_net_receive(uint8_t* buf, size_t max_len, uint16_t* out_port) {
    if (!s_eth_has_ip) return 0;

    int packet_size = ethUDP_recv.parsePacket();
    if (packet_size <= 0) return 0;

    if (static_cast<size_t>(packet_size) > max_len) {
        packet_size = static_cast<int>(max_len);
    }

    int read = ethUDP_recv.read(buf, packet_size);
    if (out_port) {
        *out_port = static_cast<uint16_t>(ethUDP_recv.remotePort());
    }
    return read;
}

int hal_net_receive_rtcm(uint8_t* buf, size_t max_len, uint16_t* out_port) {
    if (!s_eth_has_ip) return 0;

    int packet_size = ethUDP_rtcm.parsePacket();
    if (packet_size <= 0) return 0;

    if (static_cast<size_t>(packet_size) > max_len) {
        packet_size = static_cast<int>(max_len);
    }

    int read = ethUDP_rtcm.read(buf, packet_size);
    if (out_port) {
        *out_port = static_cast<uint16_t>(ethUDP_rtcm.remotePort());
    }
    return read;
}

bool hal_net_is_connected(void) {
    return s_eth_has_ip;
}

bool hal_net_detected(void) {
    return s_w5500_detected;
}

void hal_net_set_static_config(uint32_t ip, uint32_t gw, uint32_t subnet) {
    s_local_ip = u32ToIp(ip);
    s_gateway = u32ToIp(gw);
    s_subnet = u32ToIp(subnet);
}

void hal_net_set_dhcp(void) {
    s_local_ip = IPAddress(0, 0, 0, 0);
    s_gateway = IPAddress(0, 0, 0, 0);
    s_subnet  = IPAddress(0, 0, 0, 0);
}

void hal_net_apply_config(void) {
    // Stop UDP sockets
    ethUDP_recv.stop();
    ethUDP_send.stop();
    ethUDP_rtcm.stop();
    s_eth_has_ip = false;

    if (s_local_ip != IPAddress(0, 0, 0, 0)) {
        // Static IP mode — ETH.config() changes IP immediately
        if (!ETH.config(s_local_ip, s_gateway, s_subnet, s_dns, s_dns)) {
            hal_log("ETH: reconfig failed");
            return;
        }
        hal_log("ETH: reconfigured static IP to %s", s_local_ip.toString().c_str());
        s_eth_has_ip = true;

        // Re-open UDP sockets
        ethUDP_recv.begin(aog_port::AGIO_LISTEN);
        ethUDP_rtcm.begin(aog_port::RTCM_LISTEN);
        ethUDP_send.begin(aog_port::STEER);
        hal_log("ETH: UDP sockets re-opened");
    } else {
        // DHCP mode — ETH.end()/begin() is NOT reliable on RTL8201
        hal_log("ETH: DHCP mode — reboot required to take effect");
    }
}

bool hal_net_restart(void) {
    // NOTE: ETH.end()/begin() crashes on RTL8201 (LoadProhibited).
    // Use hal_net_apply_config() for runtime IP changes.
    hal_log("ETH: full restart not supported — use apply_config");
    return hal_net_is_connected();
}

uint32_t hal_net_get_ip(void) {
    if (s_eth_has_ip) {
        return ipToU32(ETH.localIP());
    }
    return ipToU32(s_local_ip);
}

uint32_t hal_net_get_gateway(void) {
    if (s_eth_has_ip) {
        return ipToU32(ETH.gatewayIP());
    }
    return ipToU32(s_gateway);
}

uint32_t hal_net_get_subnet(void) {
    if (s_eth_has_ip) {
        return ipToU32(ETH.subnetMask());
    }
    return ipToU32(s_subnet);
}

bool hal_net_link_up(void) {
    return s_eth_link_up;
}

uint32_t hal_net_get_dns(void) {
    return ipToU32(s_dns);
}

void hal_net_get_mac(uint8_t* mac_out) {
    if (!mac_out) return;
    if (s_eth_has_ip) {
        ETH.macAddress(mac_out);
    } else {
        // Return zeros if not connected
        std::memset(mac_out, 0, 6);
    }
}

uint8_t hal_net_link_speed(void) {
    if (!s_eth_link_up) return 0;
    return static_cast<uint8_t>(ETH.linkSpeed());
}

bool hal_net_full_duplex(void) {
    if (!s_eth_link_up) return false;
    return ETH.fullDuplex();
}
