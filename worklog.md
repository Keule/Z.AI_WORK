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

---
Task ID: 2
Agent: Main
Task: Add cfg_keys generic interface — all modules report their settable params

Work Log:
- Added CfgKeyDef type and cfg_keys() callback to ModuleOps2
- Updated cmd_module.cpp to iterate cfg_keys() and display key=value pairs in module show
- Updated all 13 modules with cfg_keys callback (7 with actual keys, 6 with nullptr)

Stage Summary:
- module show XY now shows all settable key/value pairs under Config: section
- Generic mechanism — each module self-reports its editable keys

---
Task ID: 3
Agent: Main
Task: Add diag_info — human-readable health reasons for all modules

Work Log:
- Added diag_info() callback (void, returns void) to ModuleOps2 struct
- Updated cmd_module.cpp: module show calls diag_info() after Health line; module debug outputs via s_cli_out instead of PASS/FAIL
- Implemented diag_info() for all 13 modules with module-specific diagnostic messages
- Rewrote all debug() functions to output via s_cli_out instead of LOGI/hal_log
- Fixed missing #include "cli.h" in 11 module files for Stream type
- Build successful, pushed to main (9240536)

Stage Summary:
- module show NTRIP now shows: "Reason: IDLE — not connected" or "Reason: connected, RTCM flowing (123 ms ago)"
- module show ETH shows: "Reason: link up, 100 Mbps full-duplex, IP 192.168.2.64"
- module debug XY now outputs full verbose diagnostic report in CLI (not just PASS/FAIL in log)
- All 13 modules have meaningful diag_info output
- Build: SUCCESS, RAM: 25.9%, Flash: 45.6%

---
Task ID: 4
Agent: Main
Task: Implement SPI_SHARED module — SPI as deactivatable module with auto DIRECT/SHARED mode

Work Log:
- Added ModuleId::SPI_SHARED to module_interface.h (before COUNT sentinel, now 14 modules)
- Created mod_spi.h with consumer registration API:
  - spi_shared_add_consumer(ModuleId) / spi_shared_remove_consumer(ModuleId)
  - spi_shared_consumer_count(), spi_shared_is_initialized(), spi_shared_is_direct_mode()
- Created mod_spi.cpp implementing full ModuleOps2 for SPI_SHARED:
  - Consumer tracking via bitmask (bits correspond to ModuleId enum values)
  - 1st consumer → hal_sensor_spi_init() in DIRECT mode (no mutex, no CS arbitration)
  - 2nd+ consumer → switch to SHARED mode (full mutex + begin/endTransaction + CS deassert)
  - Last consumer removed → hal_sensor_spi_deinit(), bus freed
  - Going from 2→1 consumers → switch back to DIRECT mode
  - Module auto-managed (not in boot_order, not user-activatable)
  - Full diag_info() and debug() with SPI telemetry output
- Modified hal_spi.cpp:
  - Added s_multi_client flag controlling transfer behavior
  - spiTransfer() has two code paths:
    DIRECT: no mutex, no other-CS deassert, minimal overhead
    SHARED: mutex + all-CS deassert + begin/endTransaction per device
  - Added hal_sensor_spi_set_multi_client() and hal_sensor_spi_is_multi_client()
- Modified hal.h: declared new SPI mode functions
- Modified mod_imu.cpp, mod_was.cpp, mod_actuator.cpp:
  - activate() calls spi_shared_add_consumer() BEFORE hal_xxx_begin()
  - deactivate() calls spi_shared_remove_consumer() + resets state
- Modified module_system.cpp:
  - Registered mod_spi_shared_ops in s_ops_table (must match ModuleId order)
  - Added freshness timeout 0 for SPI_SHARED (infrastructure, no timeout)
- Modified hal_init.cpp:
  - Removed hal_sensor_spi_init() from hal_esp32_init_all()
  - SPI bus now fully managed by module system

Stage Summary:
- SPI bus lifecycle fully managed by module system
- DIRECT mode eliminates mutex overhead for single-SPI-device configurations
- SHARED mode activates automatically when 2+ devices need SPI
- Mode transitions are seamless and automatic
- `module show SPI_SHARED` shows mode, consumer count, bus state
- `module debug SPI_SHARED` shows full telemetry (utilization, switches, deadlines)
- Diagnostic boot paths (imu_bringup, gnss_buildup) still work via direct HAL calls
- Boot: IMU activates first → DIRECT mode → WAS activates → SHARED mode → ACTUATOR activates
