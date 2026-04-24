/**
 * @file cmd_gnss.cpp
 * @brief CLI gnss/uart commands — UM980 UART setup, live console.
 *
 * Split from cli.cpp (Step 7).
 */

#include "cli.h"
#include "um980_uart_setup.h"

#include <Arduino.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern Stream* s_cli_out;

namespace {

static bool parseOnOff(const char* text, bool* out_value) {
    if (!text || !out_value) return false;
    if (std::strcmp(text, "on") == 0 || std::strcmp(text, "1") == 0) {
        *out_value = true;
        return true;
    }
    if (std::strcmp(text, "off") == 0 || std::strcmp(text, "0") == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

static bool parseUartPort(const char* text, uint8_t* out_port) {
    if (!text || !out_port) return false;
    if (std::strcmp(text, "a") == 0 || std::strcmp(text, "A") == 0) {
        *out_port = 0;
        return true;
    }
    if (std::strcmp(text, "b") == 0 || std::strcmp(text, "B") == 0) {
        *out_port = 1;
        return true;
    }
    return false;
}

void cliCmdUart(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->println("usage: uart <show|apply|set|console>");
        return;
    }

    if (std::strcmp(argv[1], "show") == 0) {
        const Um980UartSetup setup = um980SetupGet();
        s_cli_out->println("UM980 UART setup:");
        s_cli_out->printf("  A: baud=%lu swap=%s console=%s\n",
                      static_cast<unsigned long>(setup.baud_a),
                      setup.swap_a ? "ON" : "OFF",
                      setup.console_a ? "ON" : "OFF");
        s_cli_out->printf("  B: baud=%lu swap=%s console=%s\n",
                      static_cast<unsigned long>(setup.baud_b),
                      setup.swap_b ? "ON" : "OFF",
                      setup.console_b ? "ON" : "OFF");
        return;
    }

    if (std::strcmp(argv[1], "apply") == 0) {
        if (argc < 3 || std::strcmp(argv[2], "all") == 0) {
            const bool ok = um980SetupApply();
            s_cli_out->printf("UM980 UART apply all -> %s\n", ok ? "OK" : "ERROR");
            return;
        }
        uint8_t port = 0;
        if (!parseUartPort(argv[2], &port)) {
            s_cli_out->println("usage: uart apply <a|b|all>");
            return;
        }
        const bool ok = um980SetupApplyPort(port);
        s_cli_out->printf("UM980 UART apply %c -> %s\n", port == 0 ? 'A' : 'B', ok ? "OK" : "ERROR");
        return;
    }

    if (std::strcmp(argv[1], "set") == 0) {
        if (argc < 5) {
            s_cli_out->println("usage: uart set <a|b> <baud|swap> <value>");
            return;
        }
        uint8_t port = 0;
        if (!parseUartPort(argv[2], &port)) {
            s_cli_out->println("usage: uart set <a|b> <baud|swap> <value>");
            return;
        }
        if (std::strcmp(argv[3], "baud") == 0) {
            const uint32_t baud = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 10));
            if (baud == 0) {
                s_cli_out->println("ERROR: invalid baud value.");
                return;
            }
            um980SetupSetBaud(port, baud);
            s_cli_out->printf("UM980 UART %c baud set to %lu (pending apply).\n",
                          port == 0 ? 'A' : 'B',
                          static_cast<unsigned long>(baud));
            return;
        }
        if (std::strcmp(argv[3], "swap") == 0) {
            bool enabled = false;
            if (!parseOnOff(argv[4], &enabled)) {
                s_cli_out->println("usage: uart set <a|b> swap <on|off>");
                return;
            }
            um980SetupSetSwap(port, enabled);
            s_cli_out->printf("UM980 UART %c swap set to %s (pending apply).\n",
                          port == 0 ? 'A' : 'B',
                          enabled ? "ON" : "OFF");
            return;
        }
        s_cli_out->println("usage: uart set <a|b> <baud|swap> <value>");
        return;
    }

    if (std::strcmp(argv[1], "console") == 0) {
        if (argc < 4) {
            s_cli_out->println("usage: uart console <a|b> <on|off>");
            return;
        }
        uint8_t port = 0;
        if (!parseUartPort(argv[2], &port)) {
            s_cli_out->println("usage: uart console <a|b> <on|off>");
            return;
        }
        bool enabled = false;
        if (!parseOnOff(argv[3], &enabled)) {
            s_cli_out->println("usage: uart console <a|b> <on|off>");
            return;
        }
        um980SetupSetConsole(port, enabled);
        s_cli_out->printf("UM980 UART %c console -> %s\n",
                      port == 0 ? 'A' : 'B',
                      enabled ? "ON" : "OFF");
        return;
    }

    s_cli_out->println("usage: uart <show|apply|set|console>");
}

}  // namespace

void cmd_gnss_register(void) {
    (void)cliRegisterCommand("uart", &cliCmdUart, "UM980 UART setup + live console");
}
