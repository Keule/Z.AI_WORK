# ZAI_GPS Project Worklog

## Session: Phase 1 + Phase 2 ESP32 Firmware Migration & Dashboard

### Project Status Assessment
- **Phase 1 (op_mode.h/cpp cleanup)**: COMPLETE — All legacy OpMode code removed, migrated to module_interface.h API
- **Phase 2 (cleanup)**: COMPLETE — Deprecated legacy shims removed, comments cleaned up
- **Web Dashboard**: NEW — Comprehensive Next.js dashboard built for project visibility
- **Build**: PASSING (RAM 26%, Flash 46%)
- **Branch**: main (all changes pushed to origin/main)

---

### Task 1: Phase 1 Verification
- **Status**: ✅ COMPLETE (was already done in previous session)
- Verified op_mode.h and op_mode.cpp deleted from src/logic/
- Verified all 10+ files migrated from op_mode.h to module_interface.h
- Verified modeSet()/modeGet() implemented in module_system.cpp
- Verified shared_state.h created with SharedSlot<T> template
- Zero remaining op_mode/opMode functional references (only comments)
- Pushed to origin/main (commit 0ddadcb)

### Task 2: Phase 2 Cleanup
- **Status**: ✅ COMPLETE
- **Commit**: 75501d9
- Removed 8 deprecated legacy shims from module_interface.h (lines 183-202):
  - moduleIsActiveETH(), moduleIsActiveIMU(), moduleIsActiveWAS(), moduleIsActiveACT()
  - moduleIsActiveGNSS(), moduleIsActiveNTRIP(), moduleIsActiveSAFETY(), moduleIsActiveLOGGING()
- Verified zero external usage of deprecated shims (completely dead code)
- Cleaned up op_mode comments in mod_network.cpp and sd_logger_esp32.cpp
- Files changed: 3 files, +3/-26 lines
- Pushed to origin/main

### Task 3: Next.js Dashboard
- **Status**: ✅ COMPLETE
- Built comprehensive single-page dashboard at /home/z/esp32-project/
- 4 tabs: Übersicht, Module, Backlog, Downloads
- 16 module cards with search/filter/expand functionality
- 22 backlog tasks with multi-filter table
- German language UI, dark mode support
- Responsive design (mobile-first)
- Components: providers.tsx (ThemeProvider), layout.tsx (updated), page.tsx (~850 lines)
- Lint: PASSED (zero errors)
- Dev server: RUNNING on port 3000

---

### Unresolved Items / Risks
1. **Phase 2 deferred items**: GPIO mode-toggle polling (opModeGpioPoll replacement) and NVS mode persistence — deferred to future sprint
2. **GPIO 46 conflict**: IMU interrupt and SD logging switch share GPIO 46 on S3 board (TASK-034)
3. **Deprecated redirects**: fw_config.h still has backward-compat redirect from hardware_pins.h
4. **Backlog tasks**: 40+ tasks in backlog, many pending (TASK-031 through TASK-047)

### Recommended Next Steps (Priority Order)
1. Implement GPIO mode-toggle polling in task_slow (Phase 2 follow-up)
2. Add NVS mode persistence for CONFIG/WORK mode
3. Address GPIO 46 conflict (TASK-034)
4. Add real-time firmware telemetry API to the web dashboard
5. Implement ADR-STATE-001 strict state lock verification

---

## Session: Phase 1 Thread-Safety + Phase 2 Sub-Task Lifecycle

### Task 4: Phase 1 Fix — Thread-Safety for s_op_mode
- **Status**: ✅ COMPLETE
- **File**: `src/logic/module_system.cpp`
- **Changes**:
  - Added `#include <freertos/FreeRTOS.h>` and `#include <freertos/task.h>` (line 25-26)
  - Added `static portMUX_TYPE s_mode_mutex = portMUX_INITIALIZER_UNLOCKED;` after `s_op_mode` (line 100)
  - Wrapped `modeGet()` body with `portENTER_CRITICAL`/`portEXIT_CRITICAL` (lines 360-362)
  - Wrapped `modeSet()` body with spinlock: acquire on entry, release at every return point (lines 367-411)
    - Same-mode early return: unlock before return
    - WORK pipeline check fail: unlock before return
    - WORK transition: write `s_op_mode`, then unlock
    - CONFIG transition: write `s_op_mode`, then unlock
    - Fallback return: unlock before return

### Task 5: Phase 2A — Sub-Task Lifecycle API Declarations
- **Status**: ✅ COMPLETE
- **File**: `src/logic/sd_logger.h`
- **Changes**:
  - Added 4 function declarations before the `#ifdef __cplusplus` closing block (lines 131-153):
    - `bool maintTaskIsRunning(void)` — check if maint task is running
    - `bool maintTaskStart(void)` — start maint task with duplicate guard
    - `bool maintTaskStop(void)` — request graceful stop with 3s timeout
    - `void* maintTaskGetHandle(void)` — get FreeRTOS task handle for diagnostics

### Task 6: Phase 2B — Lifecycle API Implementation + Duplicate Guard + Graceful Exit + ETH Dependency
- **Status**: ✅ COMPLETE
- **File**: `src/hal_esp32/sd_logger_esp32.cpp`
- **Changes**:
  1. Added `static volatile bool s_maint_exit_requested = false;` after task handle (line 96)
  2. Added full lifecycle API implementation before `maintTaskFunc` (lines 351-434):
     - `maintTaskIsRunning()` — checks handle non-null AND exit not requested
     - `maintTaskStart()` — duplicate guard (checks handle), GPIO init, PSRAM alloc, `xTaskCreatePinnedToCore` with error handling, returns bool
     - `maintTaskStop()` — sets exit flag, polls 100ms × 30 = 3s timeout, returns bool
     - `maintTaskGetHandle()` — returns handle as `void*`
  3. Modified `maintTaskFunc()`:
     - Added exit check at TOP of `for(;;)` loop (before vTaskDelay) (lines 485-488)
     - Added ETH dependency check in NTRIP section: `else if (!hal_net_is_connected())` branch disconnects TCP on ETH link down (lines 517-522)
     - Added graceful exit cleanup after loop: close SD, disconnect NTRIP, clear handle, `vTaskDelete(nullptr)` (lines 634-660)
  4. Replaced `sdLoggerMaintInit()` body with simple delegation to `maintTaskStart()` (lines 695-697)

### Task 7: Phase 2C — task_slow owns sub-task lifecycle
- **Status**: ✅ COMPLETE
- **File**: `src/main.cpp`
- **Changes**:
  1. `bootStartTasks()` (lines 729-746):
     - NTRIP-only path: replaced `sdLoggerMaintInit()` with `maintTaskStart()` + error log
     - SD-detected path: added `maintTaskIsRunning()` check with fallback start
  2. `taskSlowFunc()` GPIO mode-toggle section (lines 997-1012):
     - After `modeSet(WORK)` succeeds: added `maintTaskStart()` (line 1002)
     - After `modeSet(CONFIG)`: added `maintTaskStop()` (line 1011)

### Task 8: Phase 2D — mod_logging uses maintTaskStart()
- **Status**: ✅ COMPLETE
- **File**: `src/logic/mod_logging.cpp`
- **Changes**:
  - Replaced `sdLoggerMaintInit()` with `maintTaskStart()` in `mod_logging_activate()` (line 86)
  - Duplicate guard in `maintTaskStart()` prevents creating a second task

### Files Modified (5 total)
| File | Lines Changed |
|------|--------------|
| `src/logic/module_system.cpp` | +12/-4 (spinlock + critical sections) |
| `src/logic/sd_logger.h` | +23 (lifecycle API declarations) |
| `src/hal_esp32/sd_logger_esp32.cpp` | +87/-31 (lifecycle impl + exit + ETH dep) |
| `src/main.cpp` | +12/-4 (boot + mode-toggle lifecycle) |
| `src/logic/mod_logging.cpp` | +2/-1 (use maintTaskStart) |

### Key Design Decisions
- **Spinlock for s_op_mode**: Uses `portMUX_TYPE` (FreeRTOS spinlock) instead of mutex because `s_op_mode` is only a single byte and critical sections are very short. The `moduleSysIsActive()` calls inside `modeSet()` are outside the critical section to avoid potential deadlocks.
- **Duplicate guard**: `maintTaskStart()` checks `s_maint_task_handle != nullptr` before creating a task. This allows multiple callers (mod_logging.activate, bootStartTasks, task_slow) to safely request task start without risking duplicate tasks.
- **Graceful exit**: `maintTaskStop()` sets a flag and polls for the task to self-delete. The task checks the flag at the TOP of each loop iteration (before vTaskDelay), then performs cleanup (close SD, disconnect NTRIP) and calls `vTaskDelete(nullptr)`.
- **ETH dependency**: NTRIP connections are now aborted when `hal_net_is_connected()` returns false, preventing wasteful TCP connect attempts when the Ethernet link is down.
