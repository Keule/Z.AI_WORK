/**
 * @file cmd_pid.cpp
 * @brief CLI PID commands — steering PID controller tuning.
 *
 * Uses mod_steer public API for PID gain read/write.
 */

#include "cli.h"
#include "mod_steer.h"
#include "module_interface.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

extern Stream* s_cli_out;

namespace {

void cliCmdPid(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->println("usage: pid <show|reset|set>");
        return;
    }

    if (std::strcmp(argv[1], "show") == 0) {
        float kp = 0, ki = 0, kd = 0;
        mod_steer_get_pid_gains(&kp, &ki, &kd);
        s_cli_out->printf("PID: Kp=%.4f Ki=%.4f Kd=%.4f\n", kp, ki, kd);
        s_cli_out->printf("     cmd=%u manual=%s\n",
                          mod_steer_get_cmd(),
                          mod_steer_manual_actuator_mode() ? "YES" : "no");
        return;
    }

    if (std::strcmp(argv[1], "reset") == 0) {
        mod_steer_reset_pid();
        s_cli_out->println("PID reset (integral cleared).");
        return;
    }

    if (std::strcmp(argv[1], "set") == 0) {
        if (argc < 5) {
            s_cli_out->println("usage: pid set <kp> <ki> <kd>");
            return;
        }
        const float kp = static_cast<float>(std::atof(argv[2]));
        const float ki = static_cast<float>(std::atof(argv[3]));
        const float kd = static_cast<float>(std::atof(argv[4]));
        mod_steer_set_pid_gains(kp, ki, kd);
        s_cli_out->printf("PID gains set: Kp=%.4f Ki=%.4f Kd=%.4f\n", kp, ki, kd);
        return;
    }

    s_cli_out->println("usage: pid <show|reset|set>");
}

}  // namespace

void cmd_pid_register(void) {
    (void)cliRegisterCommand("pid", &cliCmdPid, "PID controller tuning");
}
