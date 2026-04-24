/**
 * @file cmd_actuator.cpp
 * @brief CLI actuator commands — manual actuator test mode.
 *
 * Uses mod_actuator and mod_steer public APIs.
 */

#include "cli.h"
#include "mod_actuator.h"
#include "mod_steer.h"
#include "module_interface.h"
#include "features.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

extern Stream* s_cli_out;

namespace {

void cliCmdActuator(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->println("usage: actuator <show|manual|set>");
        return;
    }

    if (std::strcmp(argv[1], "show") == 0) {
        s_cli_out->printf("ACTUATOR: module active=%d\n",
                          moduleSysIsActive(ModuleId::ACTUATOR));
        s_cli_out->printf("  cmd=%u manual_mode=%s\n",
                          mod_actuator_get_cmd(),
                          mod_steer_manual_actuator_mode() ? "YES" : "no");
        return;
    }

    if (std::strcmp(argv[1], "manual") == 0) {
        if (argc < 3) {
            s_cli_out->println("usage: actuator manual <on|off>");
            return;
        }
        bool enable = false;
        if (std::strcmp(argv[2], "on") == 0) {
            enable = true;
        } else if (std::strcmp(argv[2], "off") == 0) {
            enable = false;
        } else {
            s_cli_out->println("usage: actuator manual <on|off>");
            return;
        }
        mod_steer_set_manual_actuator_mode(enable);
        s_cli_out->printf("Manual actuator mode: %s\n", enable ? "ON (PID disabled)" : "OFF (PID active)");
        return;
    }

    if (std::strcmp(argv[1], "set") == 0) {
        if (!mod_steer_manual_actuator_mode()) {
            s_cli_out->println("ERROR: manual mode must be ON first (actuator manual on)");
            return;
        }
        if (argc < 3) {
            s_cli_out->println("usage: actuator set <0-65535>");
            return;
        }
        const int val = std::atoi(argv[2]);
        if (val < 0 || val > 65535) {
            s_cli_out->println("ERROR: value must be 0-65535");
            return;
        }
        mod_actuator_set_cmd(static_cast<uint16_t>(val));
        s_cli_out->printf("Actuator cmd set to %u\n", static_cast<unsigned>(val));
        return;
    }

    s_cli_out->println("usage: actuator <show|manual|set>");
}

}  // namespace

void cmd_actuator_register(void) {
    (void)cliRegisterCommand("actuator", &cliCmdActuator, "Actuator manual test mode");
}
