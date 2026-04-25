/**
 * @file cmd_remote_console.cpp
 * @brief CLI commands for Remote Console module — "remoteconsole <on|off|show|port>"
 *
 * Quick-access shortcuts that wrap the generic "module REMOTE_CONSOLE ..." commands.
 */

#include "cli.h"
#include "mod_remote_console.h"
#include "module_interface.h"
#include "debug/DebugConsole.h"

#include "features.h"
#include <cstdio>
#include <cstring>

extern Stream* s_cli_out;

static void cliCmdRemoteConsole(int argc, char* argv[]) {
#if !FEAT_ENABLED(FEAT_COMPILED_REMOTE_CONSOLE)
    s_cli_out->printf("Remote console not compiled in (FEAT_REMOTE_CONSOLE)\r\n");
    (void)argc;
    (void)argv;
    return;
#endif

    if (argc < 2) {
        s_cli_out->printf("Usage: remoteconsole <on|off|show|port N>\r\n");
        return;
    }

    const char* sub = argv[1];

    if (std::strcmp(sub, "on") == 0) {
        if (moduleSysIsActive(ModuleId::REMOTE_CONSOLE)) {
            s_cli_out->printf("Remote console already active (port %u)\r\n", DBG.getTcpPort());
            return;
        }
        if (moduleSysActivate(ModuleId::REMOTE_CONSOLE)) {
            s_cli_out->printf("Remote console enabled on port %u\r\n", DBG.getTcpPort());
        } else {
            s_cli_out->printf("Failed to activate (check 'module REMOTE_CONSOLE show')\r\n");
        }
    } else if (std::strcmp(sub, "off") == 0) {
        if (!moduleSysIsActive(ModuleId::REMOTE_CONSOLE)) {
            s_cli_out->printf("Remote console already inactive\r\n");
            return;
        }
        moduleSysDeactivate(ModuleId::REMOTE_CONSOLE);
        s_cli_out->printf("Remote console disabled\r\n");
    } else if (std::strcmp(sub, "show") == 0) {
        const auto* m = moduleSysGet(ModuleId::REMOTE_CONSOLE);
        if (!m) {
            s_cli_out->printf("Remote console module not found\r\n");
            return;
        }
        s_cli_out->printf("Remote Console Status:\r\n");
        s_cli_out->printf("  Active:      %s\r\n", m->active ? "yes" : "no");
        s_cli_out->printf("  Compiled:    %s\r\n", feat::remote_console() ? "yes" : "no");
        s_cli_out->printf("  Port:        %u\r\n", DBG.getTcpPort());
        s_cli_out->printf("  TCP enabled: %s\r\n", DBG.isTcpEnabled() ? "yes" : "no");
        s_cli_out->printf("  Client:      %s\r\n", DBG.isTcpClientConnected() ? "CONNECTED" : "none");
        s_cli_out->printf("  TX: %lu  RX: %lu  Drops: %lu  Connects: %lu\r\n",
                          (unsigned long)DBG.getTcpBytesWritten(),
                          (unsigned long)DBG.getTcpBytesRead(),
                          (unsigned long)DBG.getTcpDropCount(),
                          (unsigned long)DBG.getTcpConnectCount());
    } else if (std::strcmp(sub, "port") == 0) {
        if (argc < 3) {
            s_cli_out->printf("Usage: remoteconsole port <1-65535>\r\n");
            return;
        }
        int p = std::atoi(argv[2]);
        if (p < 1 || p > 65535) {
            s_cli_out->printf("Port must be 1-65535\r\n");
            return;
        }
        // Set via module config system
        const auto* ops = moduleSysOps(ModuleId::REMOTE_CONSOLE);
        if (ops && ops->cfg_set) {
            char val[8];
            std::snprintf(val, sizeof(val), "%d", p);
            if (ops->cfg_set("port", val)) {
                s_cli_out->printf("Port set to %u (use 'remoteconsole off && remoteconsole on' to apply)\r\n",
                                  (unsigned)p);
            }
        }
    } else {
        s_cli_out->printf("Unknown sub-command: %s\r\n", sub);
        s_cli_out->printf("Usage: remoteconsole <on|off|show|port N>\r\n");
    }
}

void cmd_remote_console_register(void) {
    (void)cliRegisterCommand("remoteconsole", &cliCmdRemoteConsole,
                             "Remote TCP console control (on|off|show|port)");
}
