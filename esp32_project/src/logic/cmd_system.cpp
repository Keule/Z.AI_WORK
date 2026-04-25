/**
 * @file cmd_system.cpp
 * @brief CLI system commands — help, version, uptime, free, tasks, restart, mode, setup.
 *
 * Split from cli.cpp (Step 7).
 */

#include "cli.h"
#include "module_interface.h"
#include "setup_wizard.h"

#include <Arduino.h>
#include <cstring>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>

// CLI output stream accessor (defined in cli.cpp)
extern Stream* s_cli_out;

namespace {

void cliCmdHelp(int, char**) {
    cliPrintHelp();
}

void cliCmdVersion(int, char**) {
    s_cli_out->printf("AgSteer Build: %s %s\n", __DATE__, __TIME__);
}

void cliCmdUptime(int, char**) {
    const uint32_t sec = millis() / 1000UL;
    const uint32_t h = sec / 3600UL;
    const uint32_t m = (sec % 3600UL) / 60UL;
    const uint32_t s = sec % 60UL;
    s_cli_out->printf("Uptime: %luh %lum %lus\n",
                  static_cast<unsigned long>(h),
                  static_cast<unsigned long>(m),
                  static_cast<unsigned long>(s));
}

void cliCmdFree(int, char**) {
    s_cli_out->printf("Heap: %lu KB free (largest: %lu KB) PSRAM: %lu KB free\n",
                  static_cast<unsigned long>(ESP.getFreeHeap() / 1024UL),
                  static_cast<unsigned long>(ESP.getMaxAllocHeap() / 1024UL),
                  static_cast<unsigned long>(ESP.getFreePsram() / 1024UL));
}

void cliCmdTasks(int, char**) {
#if (configUSE_TRACE_FACILITY == 1)
    static char task_list[1024];
    task_list[0] = '\0';
    vTaskList(task_list);
    s_cli_out->println("Task         State Prio Stack Num");
    s_cli_out->print(task_list);
#else
    s_cli_out->printf("Tasks: %lu\n", static_cast<unsigned long>(uxTaskGetNumberOfTasks()));
    s_cli_out->println("Task listing unavailable (configUSE_TRACE_FACILITY=0)");
#endif
}

void cliCmdRestart(int, char**) {
    s_cli_out->println("Restarting...");
    s_cli_out->flush();
    ESP.restart();
}

void cliCmdMode(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->printf("Mode: %s\n", modeToString(modeGet()));
        return;
    }

    if (strcmp(argv[1], "work") == 0) {
        const bool ok = modeSet(OpMode::WORK);
        s_cli_out->printf("Mode -> WORK: %s\n", ok ? "OK" : "ABGELEHNT");
        if (!ok) {
            s_cli_out->println("Hinweis: WORK erfordert Pipeline bereit (IMU+WAS+ACT+SAFETY+STEER)");
        }
        return;
    }

    if (strcmp(argv[1], "config") == 0) {
        const bool ok = modeSet(OpMode::CONFIG);
        s_cli_out->printf("Mode -> CONFIG: %s\n", ok ? "OK" : "ABGELEHNT");
        return;
    }

    s_cli_out->println("usage: mode [work|config]");
}

void cliCmdSetup(int argc, char** argv) {
    (void)argc;
    (void)argv;
    setupWizardRequestStart();
    s_cli_out->println("Setup wizard requested. It will start in loop context.");
}

}  // namespace

void cmd_system_register(void) {
    (void)cliRegisterCommand("help",    &cliCmdHelp,    "List all commands");
    (void)cliRegisterCommand("version", &cliCmdVersion,  "Show firmware build version");
    (void)cliRegisterCommand("uptime",  &cliCmdUptime,   "Show uptime (h m s)");
    (void)cliRegisterCommand("free",    &cliCmdFree,     "Show heap/PSRAM memory");
    (void)cliRegisterCommand("tasks",   &cliCmdTasks,    "Show FreeRTOS task list");
    (void)cliRegisterCommand("restart", &cliCmdRestart,  "Restart ESP32");
    (void)cliRegisterCommand("mode",    &cliCmdMode,     "Operating mode [work|config] (ADR-007)");
    (void)cliRegisterCommand("setup",   &cliCmdSetup,    "Start setup wizard");
}
