/**
 * @file cmd_actuator.cpp
 * @brief CLI actuator command — status, test, stop.
 *
 * Split from cli.cpp (Step 7).
 */

#include "cli.h"
#include "control.h"
#include "hal/hal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern Stream* s_cli_out;

namespace {

void cliCmdActuator(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->println("usage: actuator <status|test>");
        return;
    }

    if (std::strcmp(argv[1], "status") == 0) {
        s_cli_out->printf("Actuator manual mode: %s\n", controlManualActuatorMode() ? "ON" : "OFF");
        s_cli_out->printf("Safety: %s\n", hal_safety_ok() ? "OK" : "KICK");
        return;
    }

    if (std::strcmp(argv[1], "test") == 0) {
        if (argc < 3) {
            s_cli_out->println("usage: actuator test <pwm|stop> [value]");
            return;
        }
        if (std::strcmp(argv[2], "stop") == 0) {
            controlSetManualActuatorMode(false);
            hal_actuator_write(0);
            s_cli_out->println("Actuator stopped. Manual mode OFF.");
            return;
        }
        if (std::strcmp(argv[2], "pwm") == 0 && argc >= 4) {
            int value = std::atoi(argv[3]);
            if (value < 0) value = 0;
            if (value > 65535) value = 65535;
            controlSetManualActuatorMode(true);
            hal_actuator_write(static_cast<uint16_t>(value));
            s_cli_out->println("WARNING: manual actuator command active (PID paused).");
            s_cli_out->printf("Actuator PWM command: %d\n", value);
            return;
        }
        s_cli_out->println("usage: actuator test <pwm|stop> [value]");
        return;
    }

    s_cli_out->println("usage: actuator <status|test>");
}

}  // namespace

void cmd_actuator_register(void) {
    (void)cliRegisterCommand("actuator", &cliCmdActuator, "Actuator manual test mode");
}
