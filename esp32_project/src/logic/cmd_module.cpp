/**
 * @file cmd_module.cpp
 * @brief CLI module command — list, enable, disable, pins.
 *
 * Split from cli.cpp (Step 7).
 */

#include "cli.h"
#include "modules.h"

#include <cstdio>
#include <cstring>

extern Stream* s_cli_out;

namespace {

const char* modStateToStr(ModState s) {
    switch (s) {
        case MOD_UNAVAILABLE: return "UNAVAILABLE";
        case MOD_OFF: return "OFF";
        case MOD_ON: return "ON";
        default: return "?";
    }
}

bool parseModuleName(const char* name, FirmwareFeatureId* out_id) {
    if (!name || !out_id) return false;
    struct Entry { const char* name; FirmwareFeatureId id; };
    static const Entry kEntries[] = {
        {"imu", MOD_IMU}, {"ads", MOD_ADS}, {"act", MOD_ACT}, {"eth", MOD_ETH},
        {"gnss", MOD_GNSS}, {"ntrip", MOD_NTRIP}, {"safety", MOD_SAFETY},
        {"logsw", MOD_LOGSW}, {"sd", MOD_SD},
    };
    for (const auto& e : kEntries) {
        if (std::strcmp(name, e.name) == 0) {
            *out_id = e.id;
            return true;
        }
    }
    return false;
}

void cliCmdModule(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->println("usage: module <list|enable|disable|pins> [name]");
        return;
    }

    if (std::strcmp(argv[1], "list") == 0) {
        s_cli_out->println("Module Status:");
        for (int i = 0; i < MOD_COUNT; ++i) {
            const auto* info = moduleGetInfo(static_cast<FirmwareFeatureId>(i));
            if (!info) continue;
            s_cli_out->printf("  %-6s (%d) = %-11s pins=%u deps=%s\n",
                          info->name ? info->name : "?",
                          i,
                          modStateToStr(moduleGetState(static_cast<FirmwareFeatureId>(i))),
                          static_cast<unsigned>(info->pin_count),
                          info->deps ? "yes" : "none");
        }
        return;
    }

    if (argc < 3) {
        s_cli_out->println("usage: module <enable|disable|pins> <name>");
        return;
    }

    FirmwareFeatureId id = MOD_COUNT;
    if (!parseModuleName(argv[2], &id)) {
        s_cli_out->println("ERROR: unknown module name.");
        return;
    }

    if (std::strcmp(argv[1], "enable") == 0) {
        const bool ok = moduleActivate(id);
        s_cli_out->printf("module %s -> %s\n", argv[2], ok ? "ON" : "ERROR");
        return;
    }

    if (std::strcmp(argv[1], "disable") == 0) {
        if (id == MOD_ETH) {
            s_cli_out->println("ERROR: ETH is mandatory and cannot be disabled.");
            return;
        }
        const bool ok = moduleDeactivate(id);
        s_cli_out->printf("module %s -> %s\n", argv[2], ok ? "OFF" : "ERROR");
        return;
    }

    if (std::strcmp(argv[1], "pins") == 0) {
        const auto* info = moduleGetInfo(id);
        if (!info) {
            s_cli_out->println("ERROR: module not found.");
            return;
        }
        s_cli_out->printf("%s pins:", info->name ? info->name : argv[2]);
        if (!info->pins || info->pin_count == 0) {
            s_cli_out->println(" (none)");
            return;
        }
        for (uint8_t i = 0; i < info->pin_count; ++i) {
            s_cli_out->printf(" %d", static_cast<int>(info->pins[i]));
        }
        s_cli_out->println();
        return;
    }

    s_cli_out->println("usage: module <list|enable|disable|pins> [name]");
}

}  // namespace

void cmd_module_register(void) {
    (void)cliRegisterCommand("module", &cliCmdModule, "Module runtime control");
}
