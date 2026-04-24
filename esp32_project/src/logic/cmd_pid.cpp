/**
 * @file cmd_pid.cpp
 * @brief CLI pid command — show, set.
 *
 * Split from cli.cpp (Step 7).
 */

#include "cli.h"
#include "runtime_config.h"
#include "control.h"
#include "global_state.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern Stream* s_cli_out;

namespace {

void cliCmdPid(int argc, char** argv) {
    RuntimeConfig& cfg = softConfigGet();
    if (argc < 2) {
        s_cli_out->println("usage: pid <show|set>");
        return;
    }

    if (std::strcmp(argv[1], "show") == 0) {
        float kp = 0.0f, ki = 0.0f, kd = 0.0f;
        uint8_t min_pwm = 0;
        uint8_t high_pwm = 0;
        {
            StateLock lock;
            min_pwm = g_nav.pid.settings_min_pwm;
            high_pwm = g_nav.pid.settings_high_pwm;
        }
        controlGetPidGains(&kp, &ki, &kd);
        s_cli_out->println("PID:");
        s_cli_out->printf("  Kp: %.3f\n", kp);
        s_cli_out->printf("  Ki: %.3f\n", ki);
        s_cli_out->printf("  Kd: %.3f\n", kd);
        s_cli_out->printf("  MinPWM: %u\n", static_cast<unsigned>(min_pwm));
        s_cli_out->printf("  HighPWM: %u\n", static_cast<unsigned>(high_pwm));
        s_cli_out->println("  Note: PGN 252 can overwrite runtime values.");
        return;
    }

    if (std::strcmp(argv[1], "set") == 0) {
        if (argc < 4) {
            s_cli_out->println("usage: pid set <kp|ki|kd|minpwm|highpwm> <value>");
            return;
        }

        if (std::strcmp(argv[2], "kp") == 0) {
            cfg.pid_kp = static_cast<float>(std::atof(argv[3]));
        } else if (std::strcmp(argv[2], "ki") == 0) {
            cfg.pid_ki = static_cast<float>(std::atof(argv[3]));
        } else if (std::strcmp(argv[2], "kd") == 0) {
            cfg.pid_kd = static_cast<float>(std::atof(argv[3]));
        } else if (std::strcmp(argv[2], "minpwm") == 0) {
            StateLock lock;
            g_nav.pid.settings_min_pwm = static_cast<uint8_t>(std::atoi(argv[3]));
        } else if (std::strcmp(argv[2], "highpwm") == 0) {
            StateLock lock;
            g_nav.pid.settings_high_pwm = static_cast<uint8_t>(std::atoi(argv[3]));
        } else {
            s_cli_out->println("usage: pid set <kp|ki|kd|minpwm|highpwm> <value>");
            return;
        }

        uint8_t min_pwm = 0;
        uint8_t high_pwm = 0;
        {
            StateLock lock;
            min_pwm = g_nav.pid.settings_min_pwm;
            high_pwm = g_nav.pid.settings_high_pwm;
        }
        controlSetPidGains(cfg.pid_kp, cfg.pid_ki, cfg.pid_kd);
        controlSetPidOutputLimits(static_cast<float>(min_pwm),
                                  static_cast<float>(high_pwm));
        s_cli_out->println("PID updated.");
        return;
    }

    s_cli_out->println("usage: pid <show|set>");
}

}  // namespace

void cmd_pid_register(void) {
    (void)cliRegisterCommand("pid", &cliCmdPid, "PID tuning and status");
}
