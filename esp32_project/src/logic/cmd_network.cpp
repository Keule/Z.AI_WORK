/**
 * @file cmd_network.cpp
 * @brief CLI net command — show, mode, ip, gw, mask, restart.
 *
 * Split from cli.cpp (Step 7).
 */

#include "cli.h"
#include "runtime_config.h"
#include "hal/hal.h"

#include <Arduino.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern Stream* s_cli_out;

namespace {

void printIpU32(uint32_t ip) {
    s_cli_out->printf("%u.%u.%u.%u",
                  static_cast<unsigned>((ip >> 24) & 0xFF),
                  static_cast<unsigned>((ip >> 16) & 0xFF),
                  static_cast<unsigned>((ip >> 8) & 0xFF),
                  static_cast<unsigned>(ip & 0xFF));
}

bool parseIp4(const char* text, uint32_t* out_ip) {
    if (!text || !out_ip) return false;
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(text, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    *out_ip = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
}

void cliCmdNet(int argc, char** argv) {
    RuntimeConfig& cfg = softConfigGet();
    if (argc < 2) {
        s_cli_out->println("usage: net <show|mode|ip|gw|mask|restart>");
        return;
    }

    if (std::strcmp(argv[1], "show") == 0) {
        s_cli_out->println("Network:");
        s_cli_out->printf("  Mode: %s\n", cfg.net_mode == 0 ? "DHCP" : "STATIC");
        s_cli_out->print("  IP: "); printIpU32(hal_net_get_ip()); s_cli_out->println();
        s_cli_out->print("  Mask: "); printIpU32(hal_net_get_subnet()); s_cli_out->println();
        s_cli_out->print("  Gateway: "); printIpU32(hal_net_get_gateway()); s_cli_out->println();
        s_cli_out->printf("  Link: %s\n", hal_net_link_up() ? "UP" : "DOWN");
        return;
    }

    if (std::strcmp(argv[1], "mode") == 0) {
        if (argc < 3) {
            s_cli_out->println("usage: net mode <dhcp|static>");
            return;
        }
        if (std::strcmp(argv[2], "dhcp") == 0) cfg.net_mode = 0;
        else if (std::strcmp(argv[2], "static") == 0) cfg.net_mode = 1;
        else {
            s_cli_out->println("usage: net mode <dhcp|static>");
            return;
        }
        s_cli_out->println("Network mode updated (apply with: net restart).");
        return;
    }

    if (std::strcmp(argv[1], "ip") == 0 || std::strcmp(argv[1], "gw") == 0 || std::strcmp(argv[1], "mask") == 0) {
        if (argc < 3) {
            s_cli_out->println("usage: net <ip|gw|mask> <a.b.c.d>");
            return;
        }
        uint32_t ip = 0;
        if (!parseIp4(argv[2], &ip)) {
            s_cli_out->println("ERROR: invalid IPv4 format.");
            return;
        }
        if (std::strcmp(argv[1], "ip") == 0) cfg.net_ip = ip;
        else if (std::strcmp(argv[1], "gw") == 0) cfg.net_gateway = ip;
        else cfg.net_subnet = ip;
        s_cli_out->println("Network parameter updated (apply with: net restart).");
        return;
    }

    if (std::strcmp(argv[1], "restart") == 0) {
        if (cfg.net_mode == 1) {
            hal_net_set_static_config(cfg.net_ip, cfg.net_gateway, cfg.net_subnet);
        }
        s_cli_out->print("Restarting network");
        for (int i = 0; i < 3; ++i) {
            s_cli_out->print(".");
            delay(100);
        }
        s_cli_out->println();
        const bool ok = hal_net_restart();
        s_cli_out->printf("Network restart %s\n", ok ? "OK" : "DONE (link pending)");
        return;
    }

    s_cli_out->println("usage: net <show|mode|ip|gw|mask|restart>");
}

}  // namespace

void cmd_network_register(void) {
    (void)cliRegisterCommand("net", &cliCmdNet, "Network runtime config");
}
