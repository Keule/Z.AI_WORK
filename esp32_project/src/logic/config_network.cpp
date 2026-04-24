/**
 * @file config_network.cpp
 * @brief Netzwerk-Konfigurations-Kategorie — ADR-006 (TASK-039).
 *
 * Verwaltet: net_mode, net_ip, net_gateway, net_subnet.
 * Validierung: IP-Format (4 Oktette 0-255).
 */

#include "config_network.h"
#include "config_framework.h"
#include "runtime_config.h"
#include "nvs_config.h"
#include "hal/hal.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_CFG
#include "esp_log.h"
#include "log_ext.h"

#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* TAG = "CFG-NET";

// ===================================================================
// Hilfsfunktionen
// ===================================================================

void configNetworkFormatIp(uint32_t ip, char* buf, size_t buf_size) {
    if (!buf || buf_size < 16) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }
    std::snprintf(buf, buf_size, "%u.%u.%u.%u",
                  (unsigned)((ip >> 24) & 0xFF),
                  (unsigned)((ip >> 16) & 0xFF),
                  (unsigned)((ip >> 8) & 0xFF),
                  (unsigned)(ip & 0xFF));
}

bool configNetworkParseIp(const char* text, uint32_t* out_ip) {
    if (!text || !out_ip) return false;
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(text, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    *out_ip = (static_cast<uint32_t>(a) << 24) |
              (static_cast<uint32_t>(b) << 16) |
              (static_cast<uint32_t>(c) << 8)  |
              static_cast<uint32_t>(d);
    return true;
}

// ===================================================================
// Kategorie-Operationen
// ===================================================================

static bool config_network_validate(void) {
    RuntimeConfig& cfg = softConfigGet();

    if (cfg.net_mode > 1) {
        LOGE(TAG, "Ungueltiger Netzwerk-Modus: %u (0=DHCP, 1=Static)", (unsigned)cfg.net_mode);
        return false;
    }

    // Bei Static-Modus: IP, Gateway, Subnet pruefen
    if (cfg.net_mode == 1) {
        // IP darf nicht 0.0.0.0 sein
        if (cfg.net_ip == 0) {
            LOGE(TAG, "Statische IP ist 0.0.0.0 — ungueltig");
            return false;
        }
        // Gateway sollte gesetzt sein
        if (cfg.net_gateway == 0) {
            LOGW(TAG, "Statischer Gateway ist 0.0.0.0 — moeglicherweise unbeabsichtigt");
        }
        // Subnet sollte nicht 0 sein
        if (cfg.net_subnet == 0) {
            LOGW(TAG, "Subnet ist 0.0.0.0 — ungueltig");
            return false;
        }
    }

    return true;
}

static bool config_network_apply(void) {
    RuntimeConfig& cfg = softConfigGet();
    if (cfg.net_mode == 1) {
        hal_net_set_static_config(cfg.net_ip, cfg.net_gateway, cfg.net_subnet);
    }
    LOGI(TAG, "Netzwerk-Konfiguration angewendet (Modus=%s)",
         cfg.net_mode == 0 ? "DHCP" : "Static");
    return true;
}

static bool config_network_load(void) {
    // NVS Load erfolgt zentral via nvsConfigLoad() in bootInitConfig()
    // Hier nur Verifikation dass Werte geladen wurden
    LOGI(TAG, "Netzwerk-Konfiguration aus NVS geladen (zentral)");
    return true;
}

static bool config_network_save(void) {
    // NVS Save erfolgt zentral via nvsConfigSave() in configFrameworkSaveAll()
    // Hier nur Verifikation
    LOGI(TAG, "Netzwerk-Konfiguration in NVS gespeichert (zentral)");
    return true;
}

static void config_network_show(ConfigStream output) {
    auto* out = static_cast<Stream*>(output);
    if (!out) return;
    RuntimeConfig& cfg = softConfigGet();

    out->println("=== Netzwerk ===");
    out->printf("  Modus:    %s\n", cfg.net_mode == 0 ? "DHCP" : "Static");

    char ip_buf[16];
    out->print  ("  IP:       "); configNetworkFormatIp(cfg.net_ip, ip_buf, sizeof(ip_buf)); out->println(ip_buf);
    out->print  ("  Gateway:  "); configNetworkFormatIp(cfg.net_gateway, ip_buf, sizeof(ip_buf)); out->println(ip_buf);
    out->print  ("  Subnet:   "); configNetworkFormatIp(cfg.net_subnet, ip_buf, sizeof(ip_buf)); out->println(ip_buf);
    out->printf("  Link:     %s\n", hal_net_link_up() ? "UP" : "DOWN");
}

static bool config_network_set(const char* key, const char* value) {
    if (!key || !value) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "mode") == 0) {
        if (std::strcmp(value, "dhcp") == 0) {
            cfg.net_mode = 0;
        } else if (std::strcmp(value, "static") == 0) {
            cfg.net_mode = 1;
        } else {
            uint8_t m = static_cast<uint8_t>(std::atoi(value));
            if (m > 1) return false;
            cfg.net_mode = m;
        }
        return true;
    }

    if (std::strcmp(key, "ip") == 0 || std::strcmp(key, "gw") == 0 || std::strcmp(key, "mask") == 0 || std::strcmp(key, "subnet") == 0) {
        uint32_t ip = 0;
        if (!configNetworkParseIp(value, &ip)) {
            LOGE(TAG, "Ungueltige IP-Adresse: %s", value);
            return false;
        }
        if (std::strcmp(key, "ip") == 0) {
            cfg.net_ip = ip;
        } else if (std::strcmp(key, "gw") == 0) {
            cfg.net_gateway = ip;
        } else {
            cfg.net_subnet = ip;
        }
        return true;
    }

    LOGW(TAG, "Unbekannter Key: %s", key);
    return false;
}

static bool config_network_get(const char* key, char* buf, size_t buf_size) {
    if (!key || !buf || buf_size == 0) return false;

    RuntimeConfig& cfg = softConfigGet();

    if (std::strcmp(key, "mode") == 0) {
        std::strncpy(buf, cfg.net_mode == 0 ? "dhcp" : "static", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return true;
    }

    if (std::strcmp(key, "ip") == 0) {
        configNetworkFormatIp(cfg.net_ip, buf, buf_size);
        return true;
    }
    if (std::strcmp(key, "gw") == 0 || std::strcmp(key, "gateway") == 0) {
        configNetworkFormatIp(cfg.net_gateway, buf, buf_size);
        return true;
    }
    if (std::strcmp(key, "mask") == 0 || std::strcmp(key, "subnet") == 0) {
        configNetworkFormatIp(cfg.net_subnet, buf, buf_size);
        return true;
    }

    return false;
}

// ===================================================================
// Kategorie-Deskriptor
// ===================================================================

static const ConfigCategoryOps s_network_ops = {
    CONFIG_CAT_NETWORK,
    "Netzwerk",       // Name (deutsch)
    "network",        // Name (englisch, fuer Keys)
    config_network_validate,
    config_network_apply,
    config_network_load,
    config_network_save,
    config_network_show,
    config_network_set,
    config_network_get
};

void configNetworkInit(void) {
    configFrameworkRegisterCategory(&s_network_ops);
}
