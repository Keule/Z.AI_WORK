/**
 * @file cli.cpp
 * @brief Serial Command Line Interface — Phase 0 (S0-01).
 *
 * Implements lightweight command parsing and dispatch for serial commands.
 * Command handlers are modularized in cmd_*.cpp files (Step 7).
 */

#include "cli.h"

#include "log_config.h"
#include "log_ext.h"
#include "debug/DebugConsole.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

// ===================================================================
// Output stream (shared with cmd_*.cpp files via extern)
// ===================================================================
Stream* s_cli_out = &DBG;

// ===================================================================
// Command registry
// ===================================================================
namespace {

constexpr size_t CLI_MAX_COMMANDS = 32;
constexpr size_t CLI_MAX_ARGS = 8;

using CliHandler = void (*)(int argc, char* argv[]);

struct CliCommand {
    const char* cmd = nullptr;
    CliHandler handler = nullptr;
    const char* help_short = nullptr;
};

CliCommand s_cli_cmd_table[CLI_MAX_COMMANDS] = {};
size_t s_cli_cmd_count = 0;

void cliCmdUnknown(const char* cmd) {
    s_cli_out->printf("Unknown command: %s\r\n", cmd ? cmd : "");
    s_cli_out->println("Type 'help' for available commands.");
}

void cliDispatch(int argc, char* argv[]) {
    if (argc <= 0 || argv == nullptr || argv[0] == nullptr) {
        return;
    }

    for (size_t i = 0; i < s_cli_cmd_count; ++i) {
        if (std::strcmp(argv[0], s_cli_cmd_table[i].cmd) == 0) {
            s_cli_cmd_table[i].handler(argc, argv);
            return;
        }
    }

    cliCmdUnknown(argv[0]);
}

}  // namespace

// ===================================================================
// Forward declarations for command group registration
// (defined in cmd_*.cpp files)
// ===================================================================
extern void cmd_system_register(void);
extern void cmd_config_register(void);
extern void cmd_network_register(void);
extern void cmd_ntrip_register(void);
extern void cmd_pid_register(void);
extern void cmd_module_register(void);
extern void cmd_diag_register(void);
extern void cmd_actuator_register(void);
extern void cmd_gnss_register(void);
extern void cmd_remote_console_register(void);

// ===================================================================
// Public API (cli.h)
// ===================================================================

void cliSetOutput(Stream* out) {
    s_cli_out = out ? out : &DBG;
}

void cliInit(void) {
    s_cli_cmd_count = 0;

    // Register all command groups (defined in cmd_*.cpp files)
    cmd_system_register();
    cmd_config_register();
    cmd_network_register();
    cmd_ntrip_register();
    cmd_pid_register();
    cmd_module_register();
    cmd_diag_register();
    cmd_actuator_register();
    cmd_gnss_register();
    cmd_remote_console_register();
}

bool cliRegisterCommand(const char* cmd,
                        void (*handler)(int argc, char* argv[]),
                        const char* help_short) {
    if (!cmd || !*cmd || !handler || !help_short) {
        return false;
    }

    for (size_t i = 0; i < s_cli_cmd_count; ++i) {
        if (std::strcmp(s_cli_cmd_table[i].cmd, cmd) == 0) {
            return false;
        }
    }

    if (s_cli_cmd_count >= CLI_MAX_COMMANDS) {
        return false;
    }

    s_cli_cmd_table[s_cli_cmd_count++] = {cmd, handler, help_short};
    return true;
}

void cliPrintHelp(void) {
    s_cli_out->println("Available commands:");
    for (size_t i = 0; i < s_cli_cmd_count; ++i) {
        s_cli_out->printf("  %-10s %s\r\n", s_cli_cmd_table[i].cmd, s_cli_cmd_table[i].help_short);
    }
    s_cli_out->println("  log ...    Runtime log controls");
    s_cli_out->println("  filter ... Runtime log file:line filter");
}

void cliProcessLine(const char* line) {
    if (!line) {
        return;
    }

    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    if (*line == '\0') {
        return;
    }

    if (std::strncmp(line, "log", 3) == 0 || std::strncmp(line, "filter", 6) == 0) {
        logProcessSerialCmd(line);
        return;
    }

    char buffer[128] = {0};
    std::strncpy(buffer, line, sizeof(buffer) - 1);

    char* argv[CLI_MAX_ARGS] = {};
    int argc = 0;

    char* token = std::strtok(buffer, " \t");
    while (token && argc < static_cast<int>(CLI_MAX_ARGS)) {
        argv[argc++] = token;
        token = std::strtok(nullptr, " \t");
    }

    cliDispatch(argc, argv);
}
