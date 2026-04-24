/**
 * @file cmd_module.cpp
 * @brief Generic module CLI — ADR-MODULE-002.
 *
 * Single CLI handler for ALL modules via the ModuleOps2 registry.
 * Replaces all modulspezifischen cmd_*.cpp handlers.
 *
 * Commands:
 *   module list                     List all modules + status
 *   module <name> show              State + health + config
 *   module <name> set <key> <val>   Set config value (RAM)
 *   module <name> load              Load from NVS
 *   module <name> apply             Apply config (re-init)
 *   module <name> save              Save to NVS
 *   module <name> activate          Activate (dep-check + pin-claim)
 *   module <name> deactivate        Deactivate (release resources)
 *   module <name> debug             Lifecycle test
 *   mode config | work              Switch operating mode
 */

#include "cli.h"
#include "module_interface.h"
#include "runtime_config.h"
#include "hal/hal.h"

#include <cstdio>
#include <cstring>

extern Stream* s_cli_out;

// ===================================================================
// Helpers
// ===================================================================

static void printModuleStatus(ModuleId id) {
    const auto* rt = moduleSysGet(id);
    if (!rt) return;
    const auto* ops = rt->ops;
    const char* active = rt->active ? "ON " : "OFF";
    const char* enabled = (ops->is_enabled && ops->is_enabled()) ? "Y" : "N";
    const bool healthy = moduleSysIsHealthy(id, hal_millis());
    const char* health = rt->active ? (healthy ? "OK " : "ERR") : "-- ";
    const char* det = rt->state.detected ? "Y" : "N";
    const char* qual = rt->state.quality_ok ? "Y" : "N";
    const char* err = rt->state.error_code == 0 ? "0" : "?";

    s_cli_out->printf("  %-10s %s enabled=%s det=%s q=%s err=%s healthy=%s\n",
                      ops->name, active, enabled, det, qual, err, health);
}

// ===================================================================
// module list
// ===================================================================
static void cliModuleList(void) {
    s_cli_out->printf("Mode: %s\n", modeToString(modeGet()));
    s_cli_out->println("Module Status:");
    for (int i = 0; i < static_cast<int>(ModuleId::COUNT); i++) {
        printModuleStatus(static_cast<ModuleId>(i));
    }
}

// ===================================================================
// module <name> show
// ===================================================================
static void cliModuleShow(const char* name) {
    ModuleId id = moduleIdFromName(name);
    if (id >= ModuleId::COUNT) {
        s_cli_out->printf("Unknown module: %s\n", name);
        return;
    }

    const auto* rt = moduleSysGet(id);
    const auto* ops = rt->ops;

    s_cli_out->printf("=== Module: %s ===\n", ops->name);
    s_cli_out->printf("  Active:    %s\n", rt->active ? "YES" : "NO");
    s_cli_out->printf("  Enabled:   %s\n", (ops->is_enabled && ops->is_enabled()) ? "YES" : "NO");
    s_cli_out->printf("  Detected:  %s\n", rt->state.detected ? "YES" : "NO");
    s_cli_out->printf("  Quality:   %s\n", rt->state.quality_ok ? "OK" : "BAD");
    s_cli_out->printf("  LastUpd:   %lu ms ago\n",
                      (unsigned long)(hal_millis() - rt->state.last_update_ms));
    s_cli_out->printf("  ErrorCode: %ld\n", (long)rt->state.error_code);
    s_cli_out->printf("  Healthy:   %s\n",
                      moduleSysIsHealthy(id, hal_millis()) ? "YES" : "NO");

    // Dependencies
    if (ops->deps) {
        s_cli_out->print("  Deps:      ");
        for (int d = 0; ops->deps[d] != ModuleId::COUNT; d++) {
            const char* dep_name = moduleIdToName(ops->deps[d]);
            const bool dep_ok = moduleSysIsActive(ops->deps[d]);
            s_cli_out->printf("%s(%s) ", dep_name, dep_ok ? "OK" : "MISS");
        }
        s_cli_out->println();
    } else {
        s_cli_out->println("  Deps:      none");
    }

    // Show config
    if (ops->cfg_show) {
        s_cli_out->println("  Config:");
        ops->cfg_show();
    }
}

// ===================================================================
// module <name> set <key> <val>
// ===================================================================
static void cliModuleSet(const char* name, const char* key, const char* val) {
    ModuleId id = moduleIdFromName(name);
    if (id >= ModuleId::COUNT) {
        s_cli_out->printf("Unknown module: %s\n", name);
        return;
    }
    const auto* ops = moduleSysOps(id);
    if (!ops->cfg_set) {
        s_cli_out->println("Module has no config interface.");
        return;
    }
    const bool ok = ops->cfg_set(key, val);
    s_cli_out->printf("%s.%s = %s -> %s\n", name, key, val, ok ? "OK" : "FAIL");
}

// ===================================================================
// module <name> load / save / apply
// ===================================================================
static void cliModuleLoad(const char* name) {
    ModuleId id = moduleIdFromName(name);
    if (id >= ModuleId::COUNT) { s_cli_out->printf("Unknown module: %s\n", name); return; }
    const auto* ops = moduleSysOps(id);
    if (!ops->cfg_load) { s_cli_out->println("Module has no load interface."); return; }
    s_cli_out->printf("%s: loading from NVS... ", name);
    const bool ok = ops->cfg_load();
    s_cli_out->println(ok ? "OK" : "FAIL");
}

static void cliModuleSave(const char* name) {
    ModuleId id = moduleIdFromName(name);
    if (id >= ModuleId::COUNT) { s_cli_out->printf("Unknown module: %s\n", name); return; }
    const auto* ops = moduleSysOps(id);
    if (!ops->cfg_save) { s_cli_out->println("Module has no save interface."); return; }
    s_cli_out->printf("%s: saving to NVS... ", name);
    const bool ok = ops->cfg_save();
    s_cli_out->println(ok ? "OK" : "FAIL");
}

static void cliModuleApply(const char* name) {
    ModuleId id = moduleIdFromName(name);
    if (id >= ModuleId::COUNT) { s_cli_out->printf("Unknown module: %s\n", name); return; }
    const auto* ops = moduleSysOps(id);
    if (!ops->cfg_apply) { s_cli_out->println("Module has no apply interface."); return; }
    s_cli_out->printf("%s: applying config... ", name);
    const bool ok = ops->cfg_apply();
    s_cli_out->println(ok ? "OK" : "FAIL");
}

// ===================================================================
// module <name> activate / deactivate
// ===================================================================
static void cliModuleActivate(const char* name) {
    ModuleId id = moduleIdFromName(name);
    if (id >= ModuleId::COUNT) { s_cli_out->printf("Unknown module: %s\n", name); return; }
    const bool ok = moduleSysActivate(id);
    if (ok) {
        // Auto-apply config so the module initializes with current settings
        const auto* ops = moduleSysOps(id);
        if (ops && ops->cfg_apply) {
            ops->cfg_apply();
        }
    }
    s_cli_out->printf("%s: activate -> %s\n", name, ok ? "ON" : "ERROR");
}

static void cliModuleDeactivate(const char* name) {
    ModuleId id = moduleIdFromName(name);
    if (id >= ModuleId::COUNT) { s_cli_out->printf("Unknown module: %s\n", name); return; }
    if (id == ModuleId::ETH) {
        s_cli_out->println("ERROR: ETH is mandatory and cannot be deactivated.");
        return;
    }
    const bool ok = moduleSysDeactivate(id);
    s_cli_out->printf("%s: deactivate -> %s\n", name, ok ? "OFF" : "ERROR");
}

// ===================================================================
// module <name> debug
// ===================================================================
static void cliModuleDebug(const char* name) {
    ModuleId id = moduleIdFromName(name);
    if (id >= ModuleId::COUNT) { s_cli_out->printf("Unknown module: %s\n", name); return; }
    const auto* ops = moduleSysOps(id);
    if (!ops->debug) { s_cli_out->println("Module has no debug interface."); return; }
    s_cli_out->printf("%s: running lifecycle debug...\n", name);
    const bool ok = ops->debug();
    s_cli_out->printf("%s: debug -> %s\n", name, ok ? "PASS" : "FAIL");
}

// ===================================================================
// module boot — show/toggle boot-disabled modules
// ===================================================================
static void cliModuleBootShow(void) {
    const auto& cfg = softConfigGet();
    const uint16_t mask = cfg.module_boot_disabled;
    s_cli_out->printf("Boot-disabled bitmask: 0x%04X (%u)\n", (unsigned)mask, (unsigned)mask);
    s_cli_out->println("Modules:");
    for (int i = 0; i < static_cast<int>(ModuleId::COUNT); i++) {
        const char* name = moduleIdToName(static_cast<ModuleId>(i));
        const bool disabled = (mask & (1u << i)) != 0;
        s_cli_out->printf("  %-10s %s\n", name, disabled ? "SKIP" : "ON  ");
    }
    s_cli_out->println();
    s_cli_out->println("Usage: module boot <on|off> <name>  (persisted via 'save')");
}

static void cliModuleBootSet(const char* action, const char* name) {
    if (std::strcmp(action, "on") != 0 && std::strcmp(action, "off") != 0) {
        s_cli_out->println("usage: module boot <on|off> <name>");
        return;
    }
    const bool disable = (std::strcmp(action, "off") == 0);

    ModuleId id = moduleIdFromName(name);
    if (id >= ModuleId::COUNT) {
        s_cli_out->printf("Unknown module: %s\n", name);
        return;
    }
    if (id == ModuleId::ETH && disable) {
        s_cli_out->println("ERROR: ETH is mandatory and cannot be boot-disabled.");
        return;
    }

    auto& cfg = softConfigGet();
    const uint16_t bit = 1u << static_cast<uint8_t>(id);
    if (disable) {
        cfg.module_boot_disabled |= bit;
    } else {
        cfg.module_boot_disabled &= ~bit;
    }
    s_cli_out->printf("%s: boot -> %s (use 'save' to persist to NVS)\n",
                      name, disable ? "DISABLED" : "ENABLED");
}

// ===================================================================
// mode config | work
// ===================================================================
static void cliModeSet(const char* mode_str) {
    OpMode target;
    if (std::strcmp(mode_str, "config") == 0) {
        target = OpMode::CONFIG;
    } else if (std::strcmp(mode_str, "work") == 0) {
        target = OpMode::WORK;
    } else {
        s_cli_out->printf("Unknown mode: %s (use 'config' or 'work')\n", mode_str);
        return;
    }

    const OpMode current = modeGet();
    if (target == current) {
        s_cli_out->printf("Already in mode: %s\n", modeToString(current));
        return;
    }

    const bool ok = modeSet(target);
    if (ok) {
        s_cli_out->printf("Mode: %s -> %s\n", modeToString(current), modeToString(target));
    } else {
        s_cli_out->printf("Mode transition REJECTED: %s -> %s\n",
                          modeToString(current), modeToString(target));
    }
}

// ===================================================================
// Dispatch
// ===================================================================
static void cliCmdModule(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->println("usage: module <list|show|set|load|apply|save|activate|deactivate|debug> [name] [key] [val]");
        return;
    }

    // module list
    if (std::strcmp(argv[1], "list") == 0) {
        cliModuleList();
        return;
    }

    // module boot [on|off] [name]
    if (std::strcmp(argv[1], "boot") == 0) {
        if (argc < 3) {
            cliModuleBootShow();
        } else {
            cliModuleBootSet(argv[2], argv[3] ? argv[3] : "");
        }
        return;
    }

    // module <name> <action>
    if (argc >= 3) {
        const char* name = argv[2];
        const char* action = argv[1];

        if (std::strcmp(action, "show") == 0)      { cliModuleShow(name); return; }
        if (std::strcmp(action, "activate") == 0)  { cliModuleActivate(name); return; }
        if (std::strcmp(action, "deactivate") == 0){ cliModuleDeactivate(name); return; }
        if (std::strcmp(action, "debug") == 0)     { cliModuleDebug(name); return; }
        if (std::strcmp(action, "load") == 0)      { cliModuleLoad(name); return; }
        if (std::strcmp(action, "save") == 0)      { cliModuleSave(name); return; }
        if (std::strcmp(action, "apply") == 0)     { cliModuleApply(name); return; }

        // module <name> set <key> <val>
        if (std::strcmp(action, "set") == 0 && argc >= 5) {
            cliModuleSet(name, argv[3], argv[4]);
            return;
        }
        if (std::strcmp(action, "set") == 0) {
            s_cli_out->println("usage: module <name> set <key> <val>");
            return;
        }
    }

    s_cli_out->println("usage: module <list|show|set|load|apply|save|activate|deactivate|debug> [name] [key] [val]");
}

static void cliCmdMode(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->printf("Current mode: %s\n", modeToString(modeGet()));
        s_cli_out->println("usage: mode <config|work>");
        return;
    }
    cliModeSet(argv[1]);
}

// ===================================================================
// Registration
// ===================================================================
void cmd_module_register(void) {
    (void)cliRegisterCommand("module", &cliCmdModule, "Module control (list/boot/show/set/load/apply/save/activate/deactivate/debug)");
    (void)cliRegisterCommand("mode", &cliCmdMode, "Operating mode (config|work)");
}
