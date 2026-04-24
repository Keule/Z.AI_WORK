# KI-Planer: Review-Nacharbeiten TASK-026..030

---
Task ID: plan-review-031-035
Agent: KI-Planer (Super Z, agents.md §4.1)
Task: Planung und Task-Erstellung für Review-Nacharbeiten (Findings F1-F8)

Work Log:
- Kombinierter Review gelesen (`/home/z/my-project/download/review.md`)
- agents.md §4.1 Pflichtlektüre durchgeführt (ADR-001, ADR-003, ADR-HAL-001, ADR-GNSS-001, ADR-LOG-001)
- Alle betroffenen Quelldateien gelesen: hal_impl.cpp, ntrip.cpp, hw_status.cpp, modules.cpp, modules.h, soft_config.h, main.cpp, sd_logger_esp32.cpp, beide Board-Profile, platformio.ini, backlog/index.yaml
- 3 offene Fragen an Menschen identifiziert (F1, F3, F4)
- Mensch-Entscheidungen erhalten: F1=(a), F3=Credentials-Datei, F4=anderer GPIO
- 5 Backlog-Tasks erstellt (TASK-031..035) mit vollem Kontext:
  - TASK-031: Legacy Pin-Claims harmonisieren (F1, high, EPIC-003)
  - TASK-032: hwStatusSetFlag Bug (F2, high, EPIC-001)
  - TASK-033: Credentials dateibasiertes Laden (F3, high, EPIC-003)
  - TASK-034: GPIO-46 + Profilname + NTRIP-Dep (F4/F5/F6, medium, EPIC-003)
  - TASK-035: sd_logger Doku + Prozess (F7, low, EPIC-003)
- backlog/index.yaml aktualisiert (Epics + Task-Einträge)
- Planungsdokument erstellt: `/home/z/my-project/download/Plan-Review-Nacharbeiten-TASK-026-030.md`

Stage Summary:
- 5 neue Task-Dateien unter backlog/tasks/
- backlog/index.yaml um 5 Tasks erweitert (EPIC-001 +1, EPIC-003 +4)
- Alle Tasks enthalten: Origin, Kontext, ACs, Scope, ADR-Referenzen, Invarianten, Known Traps, Merge Risk, Classification
- F8 (Feature-Gating) erfordert keine Aktion (bereits korrekt implementiert)
- Prozess §3 (Chat-Konservierung) ist durch die Task-Erstellung erfüllt

---

# TASK-030 Worklog

---
Task ID: 0
Agent: main
Task: TASK-030 — TASK-025 Backlog-Status und NTRIP-Architektur-Anpassung

Work Log:

Step 1 — backlog/tasks/TASK-025-ntrip-client-und-gnss-receiver-abstraktion.md (MODIFIED):
- Changed `Status: open` → `Status: done`

Step 2 — backlog/index.yaml (VERIFIED):
- TASK-025 status was already `done` — no change needed

Step 3 — src/logic/hw_status.h (MODIFIED):
- Added `ntrip_active` parameter to `hwStatusUpdate()` with default value `true`
- Updated doc comment to document the new parameter (TASK-030)
- Default value ensures backward compatibility for any existing callers

Step 4 — src/logic/hw_status.cpp (MODIFIED):
- Added `ntrip_active` parameter to `hwStatusUpdate()` function signature
- Added GNSS/NTRIP subsystem status block after safety check:
  - When NTRIP module is inactive, calls `hwStatusClearFlag(HW_GNSS)`
  - When NTRIP module is active, HW_GNSS status is managed by ntripTick()
    in ntrip.cpp (which calls hwStatusSetFlag with OK or WARNING)

Step 5 — src/main.cpp (MODIFIED):
- Updated hwStatusUpdate() call in commTaskFunc to pass `moduleIsActive(MOD_NTRIP)`
  as the 5th argument
- This ensures hwStatusUpdate() knows whether the NTRIP module is active

Step 6 — Feature Gating Audit (VERIFIED):
- ntrip.h: Entire content gated with `#if FEAT_ENABLED(FEAT_NTRIP)` ✓
- ntrip.cpp: Entire content gated with `#if FEAT_ENABLED(FEAT_NTRIP)` ✓
- main.cpp: All NTRIP function calls gated ✓
  - `#include "logic/ntrip.h"` not gated but safe (ntrip.h self-gates) ✓
  - `moduleIsActive(MOD_NTRIP)` not gated but safe (returns false when not compiled) ✓
  - `moduleActivate(MOD_NTRIP)` gated with `#if FEAT_ENABLED(FEAT_NTRIP)` ✓
- modules.cpp: MOD_NTRIP enum always present, feat::ntrip() check correct ✓
- sd_logger_esp32.cpp: ntripTick() gated with `#if FEAT_ENABLED(FEAT_NTRIP)` ✓
- hw_status.cpp/h: HW_GNSS flag clearing not feature-gated but safe (clears flag when inactive) ✓

Step 7 — reports/TASK-025/gpt-5.3-codex.md (NEW):
- Created Nachhol-Entwickler-Report for TASK-025 in German
- Documented scope, files, build fixes, test results, architecture decisions
- Listed open points resolved by TASK-027/028/029/030

Stage Summary:
- 4 modified files: hw_status.h, hw_status.cpp, main.cpp, TASK-025 task file
- 1 new file: reports/TASK-025/gpt-5.3-codex.md
- TASK-025 backlog status updated to `done`
- hwStatusUpdate() now considers NTRIP module active/inactive state
- HW_GNSS flag is cleared when NTRIP module is inactive
- Feature gating verified as consistent across all NTRIP-related code
- Backward compatibility maintained (default parameter value = true)

---

# TASK-029 Worklog

---
Task ID: 0
Agent: main
Task: TASK-029 — maintTask für Blocking-Ops und PSRAM-SD-Logging

Work Log:

Step 1 — src/logic/sd_logger.h (MODIFIED):
- Updated file header to document TASK-029 architecture (maintTask + PSRAM)
- Updated sdLoggerInit() doc comment: marked as legacy, recommends sdLoggerMaintInit()
- Added three new public API functions:
  - `sdLoggerMaintInit()` — creates maintTask + PSRAM ring buffer
  - `sdLoggerPsramBufferActive()` — check if PSRAM buffer is active
  - `sdLoggerPsramBufferCount()` — get PSRAM buffer fill count

Step 2 — src/logic/sd_logger.cpp (MODIFIED):
- Converted ring buffer from static array to pointer-based design (TASK-029)
- Added `s_ring_buf_static[512]` as fallback buffer (16 KB)
- Added `s_ring_buf` (active pointer), `s_ring_capacity`, `s_ring_mask` — all dynamically settable
- Added `sdLoggerSetExternalBuffer()` function to redirect ring buffer to external allocation
- Updated all ring buffer operations to use pointer + mask instead of fixed-size array:
  - `sdLoggerRecord()` uses `s_ring_mask` and `s_ring_buf[widx]`
  - `sdLoggerGetBufferCount()` uses `s_ring_capacity`
  - `sdLoggerHasRecords()` and `sdLoggerReadRecord()` use `s_ring_mask`
- Added `sdLoggerPsramBufferActive()`: checks if active buffer differs from static fallback
- Added `sdLoggerPsramBufferCount()`: returns 0 when PSRAM not active, else buffer count
- Renamed `LOG_RING_SIZE` → `LOG_RING_SIZE_DEFAULT` for clarity
- All existing consumer-side functions (`sdLoggerReadRecord`, `sdLoggerIncrementFlushed`, etc.) preserved unchanged
- File remains platform-independent (no ESP32/Arduino headers)

Step 3 — src/hal_esp32/sd_logger_esp32.cpp (REWRITTEN):
- Replaced standalone `loggerTaskFunc` with `maintTaskFunc` (TASK-029)
- Added PSRAM ring buffer allocation (`sdLoggerPsramInit()`):
  - Tries `heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM)` first (~1 MB)
  - Falls back to regular `malloc()` if PSRAM unavailable
  - Falls back to static 16 KB buffer in sd_logger.cpp if heap also fails
  - Calls `sdLoggerSetExternalBuffer()` to redirect ring buffer
- Added `sdLoggerMaintInit()` — public API:
  - Configures logging switch GPIO (same as legacy)
  - Allocates PSRAM buffer via `sdLoggerPsramInit()`
  - Creates `maintTask` on Core 0, priority 1, stack 8192 bytes
- Added ETH link monitoring (`maintEthMonitor()`):
  - Checks `hal_net_is_connected()` every 1 s
  - Logs on state transitions (UP/DOWN)
- `maintTaskFunc` main loop (1 s base interval):
  - ETH link monitoring (every iteration)
  - NTRIP state machine via `ntripTick()` (every iteration, gated by FEAT_NTRIP)
  - SD card flush (every 2nd iteration = 2 s, same logic as former loggerTask)
- Legacy `sdLoggerInit()` preserved: uses static buffer, creates task as "logger" (4 KB stack)
- Log tag changed from "SDL" to "MAINT" for maintTask messages
- Added `#include <esp_heap_caps.h>` for PSRAM allocation
- Added conditional `#include "logic/ntrip.h"` (gated by FEAT_ENABLED(FEAT_NTRIP))

Step 4 — src/main.cpp (MODIFIED):
- Updated file header: documented three tasks (commTask + maintTask + controlTask)
- Replaced `sdLoggerInit()` call with `sdLoggerMaintInit()` in setup()
- Added comprehensive comment block for Maintenance Task section
- Moved `ntripTick()` from commTask processing phase to maintTask:
  - In normal mode: `ntripTick()` runs in maintTask (not called from commTask)
  - In GNSS buildup mode: `ntripTick()` still runs from commTask (maintTask not created)
  - Conditional guard: `if (s_gnss_buildup_active) { ntripTick(); }`

Step 5 — src/logic/ntrip.h + src/logic/ntrip.cpp (MODIFIED):
- Updated `ntripTick()` doc comment: "call from maintTask (TASK-029)"
- Updated data flow comment in ntrip.cpp: ntripTick now runs in maintTask
- Updated cross-task safety comment in `ntripReadRtcm()`:
  - Old: "conn_state is only changed by ntripTick() which runs in the same task"
  - New: documents that ntripTick() runs in maintTask, stale CONNECTED check is harmless

Step 6 — src/logic/log_config.h (MODIFIED):
- Added `LOG_LEVEL_MAINT LOG_LVL_INFO` for maintTask log tag

Stage Summary:
- 5 modified files: sd_logger.h, sd_logger.cpp, sd_logger_esp32.cpp, main.cpp, ntrip.h, ntrip.cpp, log_config.h
- No new files
- Ring buffer converted from static 16 KB array to pointer-based design with PSRAM support
- New maintTask (Core 0, priority 1, 8 KB stack) replaces standalone loggerTask
- PSRAM ring buffer: 32768 × 32 bytes = 1 MB (~53 minutes at 10 Hz)
- SD logging: no longer blocks SD_SPI_BUS from controlTask (writes go to PSRAM, maintTask drains to SD)
- NTRIP connect: no longer blocks commTask (hal_tcp_connect runs in maintTask)
- ETH monitoring: link state changes logged by maintTask
- Backward compatibility: legacy `sdLoggerInit()` still works with static 16 KB buffer
- GNSS buildup mode: ntripTick() gracefully falls back to commTask when maintTask is absent

---

# TASK-028 Worklog

---
Task ID: 0
Agent: main
Task: TASK-028 — soft_config.h mit Nutzer-Defaults und RuntimeConfig

Work Log:

Step 1 — include/soft_config.h (NEW):
- Created header with `namespace cfg` containing all compile-time defaults
- String defaults use `inline constexpr const char*` (C++ compliant)
- Numeric defaults use `inline constexpr` (uint16_t, uint32_t, int8_t, bool)
- NTRIP Client defaults: empty host/mountpoint/user/password, port 2101, reconnect 5000 ms
- GNSS defaults: baud 460800, retry timeout 10000 ms
- Network defaults: DHCP on, static IP 192.168.1.70/24, gateway 192.168.1.1, DNS 8.8.8.8
- Logging defaults: 10 Hz interval (100 ms), inactive by default
- Module default states: NTRIP=1 (on), LOGSW=0 (off, switch controlled)
- NET_SUBNET uses uint8_t[4] (was typo uint8_t[3] in task spec, corrected to 4)

Step 2 — src/logic/runtime_config.h (NEW):
- Defined `RuntimeConfig` struct with mutable RAM fields for all user-configurable values
- NTRIP fields: host[64], port, mountpoint[48], user[32], password[32], reconnect_ms
- GNSS field: baud
- Logging fields: interval_ms, default_active
- Declared `softConfigLoadDefaults()`, `softConfigLoadOverrides()`, `softConfigGet()`

Step 3 — src/logic/runtime_config.cpp (NEW):
- Static `s_runtime_config` instance (zero-initialised)
- `softConfigLoadDefaults()`: copies all cfg:: defaults into RuntimeConfig via strncpy + direct assignment
- All strncpy calls include NUL-termination safety
- `softConfigLoadOverrides()`: stub returning false (future: SD card / Serial / WebUI)
- `softConfigGet()`: returns reference to global instance

Step 4 — src/main.cpp (MODIFIED):
- Added `#include "logic/runtime_config.h"`
- Added soft config init BEFORE modulesInit() in normal startup path:
  `softConfigLoadDefaults(softConfigGet())` + `softConfigLoadOverrides(softConfigGet())`
- Added soft config init in GNSS buildup path (before NTRIP section)
- NTRIP config section: replaced hardcoded "caster.example.com"/2101/"VRS"/"user"/"pass"
  with RuntimeConfig values, with fallback to example values when cfg:: defaults are empty

Step 5 — src/logic/modules.cpp (MODIFIED):
- Added `#include "soft_config.h"` for cfg:: module default states
- Modified `featureModulesInitCompiled()` initial state logic:
  - MOD_UNAVAILABLE for uncompiled modules (unchanged)
  - MOD_NTRIP initial state = cfg::MOD_DEFAULT_NTRIP (1 = ON by default)
  - MOD_LOGSW initial state = cfg::MOD_DEFAULT_LOGSW (0 = OFF by default)
  - All other compiled modules = MOD_OFF (unchanged)

Stage Summary:
- 3 new files: soft_config.h, runtime_config.h, runtime_config.cpp
- 2 modified files: main.cpp, modules.cpp
- Compile-time defaults separated from runtime-mutable configuration
- RuntimeConfig loaded at boot from cfg:: defaults, stub for future overrides
- NTRIP configuration no longer hardcoded in main.cpp — uses RuntimeConfig
- Module default states (NTRIP, LOGSW) configurable via cfg:: namespace
- Future-ready: softConfigLoadOverrides() stub ready for SD/Serial/WebUI

---

# TASK-027 Worklog

---
Task ID: 0
Agent: main
Task: TASK-027 — Modul-System mit Runtime-Aktivierung und Pin-Claim-Arbitrierung

Work Log:

Step 1 — Board Profile Feature Pin Groups:
- Added FEAT_PINS_* arrays (static constexpr int8_t[]) to ESP32-S3 board profile
  - FEAT_PINS_IMU: GPIO 46,41,15,40 (INT/RST/WAKE/CS)
  - FEAT_PINS_ADS: GPIO 18 (CS)
  - FEAT_PINS_ACT: GPIO 16 (CS)
  - FEAT_PINS_ETH: GPIO 10,11,12,9,13,14 (SCK/MISO/MOSI/CS/INT/RST)
  - FEAT_PINS_GNSS: GPIO 48,45,2,1 (UART1 TX/RX, UART2 TX/RX)
  - FEAT_PINS_NTRIP: empty (no dedicated pins)
  - FEAT_PINS_SAFETY: GPIO 4
  - FEAT_PINS_LOGSW: GPIO 46 (documented CONFLICT with IMU_INT)
- Added FEAT_PINS_*_COUNT constants for each array
- Added FEAT_DEPS_NTRIP (depends on MOD_ETH=3) and FEAT_DEPS_ACT (depends on MOD_IMU=0, MOD_ADS=1)
- Added matching arrays to ESP32 Classic board profile with -1 for unpopulated sensor pins
- Used numeric literals for dep IDs (matching FirmwareFeatureId enum values)

Step 2 — modules.h — New Feature Module System Types:
- Added ModState enum (MOD_UNAVAILABLE=-1, MOD_OFF=0, MOD_ON=1)
- Added FirmwareFeatureId enum (MOD_IMU=0..MOD_LOGSW=7, MOD_COUNT=8)
- Added FeatureModuleInfo struct (name, state, compiled, hw_detected, pins, pin_count, deps)
- Added API declarations: moduleActivate, moduleDeactivate, moduleIsActive, moduleGetState, moduleGetInfo
- All existing AOG module declarations preserved unchanged

Step 3 — hal.h + hal_impl.cpp — Pin Claim Arbitration:
- Added hal_pin_claim_add(int pin, const char* owner) → bool (C linkage)
- Added hal_pin_claim_release(const char* owner) → int (C linkage)
- Added hal_pin_claim_check(int pin) → bool (C linkage)
- Implemented all three in hal_impl.cpp with extern "C" wrappers
- hal_pin_claim_add: rejects negative pins, checks for conflicts, adds to claim table
- hal_pin_claim_release: reverse-iterates claim table, shifts entries on match
- hal_pin_claim_check: delegates to existing pinClaimFind()

Step 4 — modules.cpp — Feature Module Implementation:
- Added #include "fw_config.h" for board profile pin groups
- Added static g_features[MOD_COUNT] array with board profile pin/dep references
- Added featureModulesInitCompiled(): sets compiled flags based on feat::*() AND pin_count > 0
  - IMU→feat::imu(), ADS→feat::sensor(), ACT→feat::actor(), ETH→feat::comm()
  - GNSS→feat::gnss(), NTRIP→feat::ntrip(), SAFETY→feat::control(), LOGSW→always if pins
- Added featureModulesSyncHwDetected(): copies s_hw detection results to feature module hw_detected
- Implemented moduleActivate(): compiled check → dependency check → pin claim (skip pre-claimed) → MOD_ON
- Implemented moduleDeactivate(): releases pins via hal_pin_claim_release → MOD_OFF
- Implemented moduleIsActive(), moduleGetState(), moduleGetInfo()
- Updated modulesInit() to call featureModulesInitCompiled() and featureModulesSyncHwDetected()
- Transition behavior: pins already claimed by legacy HAL init are accepted (not re-claimed)

Step 5 — main.cpp — Default Module Activation:
- Added moduleActivate calls in setup() after modulesInit()
- Activation order: IMU, ADS, ETH, GNSS, SAFETY (no deps), then ACT (deps: IMU+ADS), then NTRIP (deps: ETH)
- NTRIP activation gated by #if FEAT_ENABLED(FEAT_NTRIP)

Stage Summary:
- New three-state hardware feature module system (ModState: UNAVAILABLE/OFF/ON)
- 8 feature modules: IMU, ADS, ACT, ETH, GNSS, NTRIP, SAFETY, LOGSW
- Runtime activation with pin-claim arbitration and dependency checking
- HAL pin-claim API exposed (hal_pin_claim_add/release/check)
- Board profiles extended with FEAT_PINS_* and FEAT_DEPS_* arrays
- Existing AogModuleInfo system preserved unchanged
- Transition: legacy HAL init pin claims are accepted during moduleActivate()
- Known pin conflict: IMU_INT (GPIO 46) and LOG_SWITCH_PIN (GPIO 46) on S3 board

---
Task ID: 0
Agent: main
Task: TASK-026 — fw_config und Board-Profile Restrukturierung

Work Log:
- Created `include/fw_config.h` as new firmware config entry point (replaces hardware_pins.h)
- Converted `include/hardware_pins.h` to deprecated redirect (includes fw_config.h)
- Cleaned up S3 board profile comment block (45 lines → 15 lines, accurate pin summary)
- Fixed S3 board profile #error message: "hardware_pins.h" → "fw_config.h"
- Fixed ESP32 classic board profile: GNSS UART comments no longer reference ESP32-S3 PSRAM constraints
- Fixed ESP32 classic board profile: LOG_SWITCH_PIN comment no longer references ESP32-S3 strapping pin
- Verified BOARD_PROFILE_NAME exists on ESP32 classic board profile
- Updated 5 source files: `#include "hardware_pins.h"` → `#include "fw_config.h"`
  - src/hal_esp32/hal_impl.cpp
  - src/hal_esp32/hal_bno085.cpp
  - src/hal_esp32/sd_logger_esp32.cpp
  - src/hal_esp32/sd_ota_esp32.cpp
  - src/main.cpp
- Fixed hal_impl.cpp header comment with correct pin assignments (S3 board)
- Fixed outdated pin comments in sd_logger_esp32.cpp and sd_ota_esp32.cpp
- Fixed ADS1118 wiring comment in hal_impl.cpp (GPIO 15/16/17 → 21/38/47)
- Fixed sensorSPI comment in hal_impl.cpp (CS=47/CS=40 → CS=40/CS=16, pins 16/15/17 → 47/21/38)
- Added deprecated redirect for `include/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h`
- Added deprecated redirect for `include/LILYGO_T_ETH_LITE_ESP32_board_pins.h`
- Verified: `grep -rn "hardware_pins" src/ include/` returns only the deprecated redirect file

Stage Summary:
- All source files now include fw_config.h instead of hardware_pins.h
- Deprecated redirect left in place for backward compatibility
- Board profile comments cleaned up and corrected for both S3 and classic ESP32
- Top-level duplicate board profile files replaced with deprecated redirects
- No logic changes, only includes/comments/rename
