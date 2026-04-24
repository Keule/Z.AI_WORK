---
Task ID: 1
Agent: Main
Task: ADR-MODULE-002 Unified Module System Refactoring

Work Log:
- Read all infrastructure files (features.h, global_state.h, hal.h, nvs_config, runtime_config, cli.h/cpp, op_mode.h/cpp, main.cpp, state_structs.h, soft_config.h, fw_config.h, cmd_module.cpp)
- Analyzed existing module implementations (imu, was, actuator, control, net, sd_logger, ntrip) against ADR-MODULE-001
- Wrote ADR-MODULE-002 (docs/adr/ADR-MODULE-002-unified-module-system.md)
- Wrote new module_interface.h with ModuleId enum (13 modules), ModuleResult, ModState, ModuleOps2, OpMode (CONFIG/WORK), ModuleRuntime, and moduleSys* API
- Wrote module_system.cpp with central registry, dependency checking, pipeline execution (input/process/output), boot activation, and mode management
- Dispatched 3 parallel subagents to write all 13 module implementations (26 files total)
  - Batch 1: mod_eth, mod_wifi, mod_bt, mod_network (transport + protocol)
  - Batch 2: mod_imu, mod_was, mod_actuator, mod_gnss, mod_safety, mod_ntrip (sensors + services)
  - Batch 3: mod_steer, mod_logging, mod_ota (logic + services)
- Rewrote cmd_module.cpp as generic CLI handler (module list/show/set/load/apply/save/activate/deactivate/debug + mode config|work)

Stage Summary:
- Created: ADR-MODULE-002, module_interface.h, module_system.cpp
- Created 26 module files (mod_*.h + mod_*.cpp for all 13 modules)
- Rewrote cmd_module.cpp with generic CLI
- Key design decisions:
  - Two modes only: CONFIG and WORK (no PAUSED, no BOOTING as persistent mode)
  - All modules have identical 15-function interface
  - mod_steer replaces control (renamed for clarity)
  - mod_network separates PGN protocol from transport
  - Each module has internal ModState with 4 fields
  - NVS keys prefixed with mod_<name>_
- NEXT STEPS: Update main.cpp to use new module system, clean up old files
