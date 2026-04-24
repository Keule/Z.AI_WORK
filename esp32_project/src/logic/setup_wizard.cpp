/**
 * @file setup_wizard.cpp
 * @brief First-boot serial setup wizard (Phase 0 / S0-08).
 */

#include "setup_wizard.h"

#include "runtime_config.h"
#include "nvs_config.h"
#include "hal/hal.h"

#include <Arduino.h>
#include <cstring>
#include <cstdlib>

namespace {

bool s_setup_wizard_pending = false;

bool readLine(char* out, size_t out_size, uint32_t timeout_ms = 60000) {
    if (!out || out_size == 0) return false;
    size_t len = 0;
    const uint32_t start = millis();
    while ((millis() - start) < timeout_ms) {
        while (Serial.available()) {
            const int ch = Serial.read();
            if (ch == '\r' || ch == '\n') {
                out[len] = '\0';
                return true;
            }
            if (len + 1 < out_size && ch >= 32 && ch <= 126) {
                out[len++] = static_cast<char>(ch);
                Serial.print(static_cast<char>(ch));
            }
        }
        delay(10);
    }
    out[len] = '\0';
    return false;
}

uint32_t parseIp(const char* s, uint32_t fallback) {
    if (!s || !*s) return fallback;
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return fallback;
    if (a > 255 || b > 255 || c > 255 || d > 255) return fallback;
    return (a << 24) | (b << 16) | (c << 8) | d;
}

}  // namespace

void setupWizardRequestStart(void) {
    s_setup_wizard_pending = true;
}

bool setupWizardConsumePending(void) {
    if (!s_setup_wizard_pending) return false;
    s_setup_wizard_pending = false;
    return true;
}

bool setupWizardRun(void) {
    RuntimeConfig& cfg = softConfigGet();
    char line[96] = {};

    Serial.println();
    Serial.println("=== AgSteer Setup-Wizard ===");
    Serial.println("Press ENTER to keep default values.");

    Serial.println("Step 1/3: Network");
    Serial.printf("Use DHCP? [Y/n]: ");
    if (readLine(line, sizeof(line))) {
        Serial.println();
        if (line[0] == 'n' || line[0] == 'N') cfg.net_mode = 1;
        else if (line[0] == 'y' || line[0] == 'Y' || line[0] == '\0') cfg.net_mode = 0;
    }

    if (cfg.net_mode == 1) {
        Serial.printf("Static IP [192.168.5.70]: ");
        if (readLine(line, sizeof(line))) {
            Serial.println();
            cfg.net_ip = parseIp(line, cfg.net_ip);
        }
        Serial.printf("Gateway [192.168.5.1]: ");
        if (readLine(line, sizeof(line))) {
            Serial.println();
            cfg.net_gateway = parseIp(line, cfg.net_gateway);
        }
        Serial.printf("Mask [255.255.255.0]: ");
        if (readLine(line, sizeof(line))) {
            Serial.println();
            cfg.net_subnet = parseIp(line, cfg.net_subnet);
        }
    }

    Serial.println("Step 2/3: NTRIP");
    Serial.printf("Caster host [%s]: ", cfg.ntrip_host);
    if (readLine(line, sizeof(line)) && line[0] != '\0') {
        Serial.println();
        std::strncpy(cfg.ntrip_host, line, sizeof(cfg.ntrip_host) - 1);
        cfg.ntrip_host[sizeof(cfg.ntrip_host) - 1] = '\0';
    }
    Serial.printf("Mountpoint [%s]: ", cfg.ntrip_mountpoint);
    if (readLine(line, sizeof(line)) && line[0] != '\0') {
        Serial.println();
        std::strncpy(cfg.ntrip_mountpoint, line, sizeof(cfg.ntrip_mountpoint) - 1);
        cfg.ntrip_mountpoint[sizeof(cfg.ntrip_mountpoint) - 1] = '\0';
    }

    Serial.println("Step 3/3: Actuator type");
    Serial.printf("Actuator type (0=SPI,1=Cytron,2=IBT2) [%u]: ", (unsigned)cfg.actuator_type);
    if (readLine(line, sizeof(line)) && line[0] != '\0') {
        Serial.println();
        cfg.actuator_type = static_cast<uint8_t>(std::atoi(line));
    }

    Serial.println("Save configuration? [Y/n]: ");
    if (!readLine(line, sizeof(line))) {
        Serial.println();
        Serial.println("Wizard timeout. No changes saved.");
        return false;
    }
    Serial.println();
    if (line[0] == 'n' || line[0] == 'N') {
        Serial.println("Setup aborted. No changes saved.");
        return false;
    }

    if (!nvsConfigSave(cfg)) {
        Serial.println("ERROR: Failed to save wizard config.");
        return false;
    }

    Serial.println("Configuration saved to NVS.");
    Serial.println("Restarting...");
    Serial.flush();
    ESP.restart();
    return true;
}
