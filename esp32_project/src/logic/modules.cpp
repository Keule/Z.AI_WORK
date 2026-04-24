/**
 * @file modules.cpp
 * @brief AgOpenGPS module registry + Hardware Feature Module System.
 *
 * This file contains two module systems:
 *
 * 1. AgOpenGPS Module Registry (existing):
 *    Bridges low-level hardware detection (HAL) with the AgOpenGPS
 *    protocol layer (PGN encoding, UDP sending).
 *
 * 2. Hardware Feature Module System (TASK-027):
 *    Three-state module system (unavailable/off/on) with runtime
 *    activation, pin-claim arbitration, and dependency checking.
 *    AOG module hw_detected is derived from feature module states.
 */

#include "modules.h"
#include "features.h"
#include "pgn_codec.h"
#include "pgn_types.h"
#include "global_state.h"
#include "hw_status.h"
#include "hal/hal.h"
#include "fw_config.h"   // Board profile pin groups (FEAT_PINS_*, FEAT_DEPS_*)
#include "soft_config.h" // cfg:: module default states (TASK-028)

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MOD
#include "esp_log.h"
#include "log_ext.h"

#include <cstdio>
#include <cstring>

// ===================================================================
// Forward declarations (static functions defined below modulesInit)
// ===================================================================
static void featureModulesInitCompiled(void);
static void featureModulesSyncHwDetected(void);
//static FeatureModuleInfo g_features[MOD_COUNT];

// ===================================================================
// Internal state
// ===================================================================

/// Module table – all modules this firmware implements
static AogModuleInfo s_modules[AOG_MOD_COUNT] = {
    { aog_src::STEER,   aog_pgn::HELLO_REPLY_STEER, aog_port::STEER,   "Steer",   false, false },
    { aog_src::GPS,     aog_pgn::HELLO_REPLY_GPS,   aog_port::GPS,     "GPS",     false, false },
    { aog_src::MACHINE, aog_src::MACHINE,           aog_port::MACHINE, "Machine", false, false },
};

/// Hardware detection results (filled by modulesInit)
static ModuleHwStatus s_hw = {};

/// Module IP (for subnet replies) – matches hal_impl.cpp default
static uint8_t s_module_ip[4] = {192, 168, 1, 70};
static uint8_t s_module_subnet[3] = {255, 255, 255};

/// Track if startup errors still need to be sent to AgIO.
static bool s_startup_errors_pending = true;

static bool isModuleActive(const AogModuleInfo& mod) {
    return mod.enabled && mod.hw_detected;
}

// ===================================================================
// Init – detect all hardware
// ===================================================================
void modulesInit(void) {
    hal_log("MODULES: === Hardware Detection ===");

    // --- Initialise feature module compiled/availability flags ---
    featureModulesInitCompiled();

    const bool mod_eth_enabled = feat::eth() && (FEAT_PINS_ETH_COUNT > 0);
    const bool mod_ads_enabled = feat::ads() && (FEAT_PINS_ADS_COUNT > 0);
    const bool mod_imu_enabled = feat::imu() && (FEAT_PINS_IMU_COUNT > 0);
    const bool mod_act_enabled = feat::act() && (FEAT_PINS_ACT_COUNT > 0);
    const bool mod_safety_enabled = feat::safety() && (FEAT_PINS_SAFETY_COUNT > 0);
    const bool mod_sd_enabled = (FEAT_PINS_SD_COUNT > 0);

    // --- Detect individual subsystems ---

    // Ethernet
    s_hw.eth_detected = mod_eth_enabled ? hal_net_detected() : true;
    hal_log("MODULES: Ethernet (W5500)    : %s", s_hw.eth_detected ? "OK" : "FAIL");

    // Steer Angle Sensor (ADS1118) – detect BEFORE IMU!
    // The ADS1118 holds DOUT LOW while converting, which would
    // cause the IMU detect to read 0x00 (false negative).
    // Detecting ADS1118 first ensures DOUT is HIGH after detection.
    s_hw.was_detected = mod_ads_enabled ? hal_steer_angle_detect() : true;

    // IMU (BNO085)
    if (mod_imu_enabled) {
        HalImuDetectStats imu_ds = {};
        s_hw.imu_detected = hal_imu_detect_boot_qualified(&imu_ds);
        hal_log("MODULES: IMU boot detect      : %s (ok=%u/%u ff=%u zero=%u last=0x%02X)",
                s_hw.imu_detected ? "OK" : "FAIL",
                (unsigned)imu_ds.ok_count,
                (unsigned)imu_ds.samples,
                (unsigned)imu_ds.ff_count,
                (unsigned)imu_ds.zero_count,
                (unsigned)imu_ds.last_response);
    } else {
        s_hw.imu_detected = true;
    }

    // Actuator
    s_hw.actuator_detected = mod_act_enabled ? hal_actuator_detect() : true;

    // Safety circuit
    s_hw.safety_ok = mod_safety_enabled ? hal_safety_ok() : true;
    hal_log("MODULES: Safety Circuit       : %s", s_hw.safety_ok ? "OK" : "KICK");

    // SD card presence (one-shot boot init/mount probe)
    s_hw.sd_present = mod_sd_enabled ? hal_sd_card_present() : false;
    hal_log("MODULES: SD card presence     : %s", s_hw.sd_present ? "PRESENT" : "MISSING");

    // --- Derive module enablement + hw_detected from feature profiles ---
    s_modules[AOG_MOD_STEER].enabled   = feat::act() && feat::safety();
    s_modules[AOG_MOD_GPS].enabled     = feat::ads();
    s_modules[AOG_MOD_MACHINE].enabled = feat::act();

    // Steer Module: needs Ethernet + WAS + IMU + Actuator + Safety
    s_modules[AOG_MOD_STEER].hw_detected =
        s_hw.eth_detected && s_hw.was_detected && s_hw.imu_detected &&
        s_hw.actuator_detected && s_hw.safety_ok;

    // GPS Module: needs Ethernet + WAS + IMU
    s_modules[AOG_MOD_GPS].hw_detected =
        s_hw.eth_detected && s_hw.was_detected && s_hw.imu_detected;

    // Machine Module: needs Ethernet + actuator path
    s_modules[AOG_MOD_MACHINE].hw_detected =
        s_hw.eth_detected && s_hw.actuator_detected;

    // --- Log module summary ---
    hal_log("MODULES: === Module Summary ===");
    for (uint8_t i = 0; i < AOG_MOD_COUNT; i++) {
        hal_log("MODULES:   [%u] %s (Src=0x%02X, Port=%u) enabled=%s hw=%s",
                (unsigned)i,
                s_modules[i].name,
                s_modules[i].src_id,
                (unsigned)s_modules[i].port,
                s_modules[i].enabled ? "Y" : "N",
                s_modules[i].hw_detected ? "OK" : "FAIL");
    }
    hal_log("MODULES: === Detection Complete ===");

    // --- Sync feature module hw_detected from detection results ---
    featureModulesSyncHwDetected();
}

// ===================================================================
// Accessors
// ===================================================================
uint8_t modulesCount(void) {
    return AOG_MOD_COUNT;
}

const AogModuleInfo* modulesGet(AogModuleId id) {
    if (id >= AOG_MOD_COUNT) return nullptr;
    return &s_modules[id];
}

const ModuleHwStatus* modulesGetHwStatus(void) {
    return &s_hw;
}

bool modulesHwOk(AogModuleId id) {
    if (id >= AOG_MOD_COUNT) return false;
    return s_modules[id].hw_detected;
}

// ===================================================================
// Send hello replies for ALL enabled modules
// ===================================================================
void modulesSendHellos(void) {
    uint8_t buf[64];

    for (uint8_t i = 0; i < AOG_MOD_COUNT; i++) {
        if (!isModuleActive(s_modules[i])) continue;

        const AogModuleInfo& mod = s_modules[i];
        size_t len = 0;
        const char* label = nullptr;

        if (mod.hello_pgn == aog_pgn::HELLO_REPLY_STEER) {
            // Steer hello: PGN=0x7E, Len=5
            // Payload: steerAngle×100(2) + sensorCounts(2) + switchByte(1)
            StateLock lock;
            int16_t angle = static_cast<int16_t>(g_nav.steer.steer_angle_deg * 100.0f);
            uint16_t counts = static_cast<uint16_t>(g_nav.steer.steer_angle_raw);
            uint8_t sw = 0;
            if (!g_nav.safety.safety_ok)   sw |= 0x80;  // bit 7 = safety
            if (g_nav.sw.work_switch) sw |= 0x01;  // bit 0 = work switch
            if (g_nav.sw.steer_switch) sw |= 0x02; // bit 1 = steer switch
            len = pgnEncodeHelloReplySteer(buf, sizeof(buf), angle, counts, sw);
            label = "SteerHello";
        } else if (mod.hello_pgn == aog_pgn::HELLO_REPLY_GPS) {
            len = pgnEncodeHelloReplyGps(buf, sizeof(buf));
            label = "GpsHello";
        } else {
            // Generic hello reply: 5-byte zero payload, PGN = module hello PGN.
            // Used for modules without dedicated hello payload codec.
            uint8_t payload[5] = {0, 0, 0, 0, 0};
            len = pgnBuildFrame(buf, sizeof(buf), mod.src_id, mod.hello_pgn,
                                payload, sizeof(payload));
            label = "GenericHello";
        }

        if (len > 0) {
            hal_net_send(buf, len, mod.port);
            pgnHexDump(label, buf, len);
            hal_log("MODULE: sent %s hello reply (%zu bytes, src=0x%02X, port=%u)",
                    mod.name, len, mod.src_id, (unsigned)mod.port);
        }
    }
}

// ===================================================================
// Send subnet replies for ALL enabled modules
// ===================================================================
void modulesSendSubnetReplies(void) {
    uint8_t buf[64];

    for (uint8_t i = 0; i < AOG_MOD_COUNT; i++) {
        if (!isModuleActive(s_modules[i])) continue;

        const AogModuleInfo& mod = s_modules[i];
        size_t len = pgnEncodeSubnetReply(buf, sizeof(buf),
                                           mod.src_id,
                                           s_module_ip, s_module_subnet);
        if (len > 0) {
            hal_net_send(buf, len, mod.port);
            hal_log("MODULE: sent %s subnet reply (src=0x%02X, port=%u)",
                    mod.name, mod.src_id, (unsigned)mod.port);
        }
    }
}

// ===================================================================
// Send startup error messages for failed hardware
// ===================================================================
void modulesSendStartupErrors(void) {
    if (!s_startup_errors_pending) return;
    const bool net_connected = hal_net_is_connected();
    if (!net_connected) {
        hal_log("MODULES: startup errors pending (network down, defer UDP report)");
    }

    hal_log("MODULES: === Startup Error Report ===");

    // --- Module-level subsystem checks (active profile only) ---
    for (uint8_t i = 0; i < AOG_MOD_COUNT; i++) {
        if (!s_modules[i].enabled) continue;
        const AogModuleInfo& mod = s_modules[i];

        if (!s_hw.eth_detected) {
            hwStatusSendClassifiedMessage(mod.src_id, HW_ERR_CLASS_STARTUP, HW_ERR_PRIO_P1,
                                          aog_hwmsg::DURATION_PERSIST,
                                          "ERR Ethernet: W5500 Not Detected");
        }

        if (i == AOG_MOD_STEER || i == AOG_MOD_GPS) {
            if (!s_hw.was_detected) {
                hwStatusSendClassifiedMessage(mod.src_id, HW_ERR_CLASS_STARTUP, HW_ERR_PRIO_P1,
                                              aog_hwmsg::DURATION_PERSIST,
                                              "ERR Steer Angle Sensor: Not Detected");
            }
            if (!s_hw.imu_detected) {
                hwStatusSendClassifiedMessage(mod.src_id, HW_ERR_CLASS_STARTUP, HW_ERR_PRIO_P1,
                                              aog_hwmsg::DURATION_PERSIST,
                                              "ERR IMU (BNO085): Not Detected");
            }
        }

        if (i == AOG_MOD_STEER || i == AOG_MOD_MACHINE) {
            if (!s_hw.actuator_detected) {
                hwStatusSendClassifiedMessage(mod.src_id, HW_ERR_CLASS_STARTUP, HW_ERR_PRIO_P1,
                                              aog_hwmsg::DURATION_PERSIST,
                                              "ERR Actuator: Not Detected");
            }
        }

        if (i == AOG_MOD_STEER && !s_hw.safety_ok) {
            hwStatusSendClassifiedMessage(mod.src_id, HW_ERR_CLASS_STARTUP, HW_ERR_PRIO_P1,
                                          aog_hwmsg::DURATION_PERSIST,
                                          "ERR Safety Circuit: KICK Engaged");
        }

        char msg[64];
        std::snprintf(msg, sizeof(msg), "ERR Module %s: Not Available", mod.name);
        if (!mod.hw_detected) {
            hwStatusSendClassifiedMessage(mod.src_id, HW_ERR_CLASS_STARTUP, HW_ERR_PRIO_P1,
                                          aog_hwmsg::DURATION_PERSIST, "%s", msg);
        }
    }

    hal_log("MODULES: === Startup Error Report Complete (%s) ===",
            hal_net_is_connected() ? "UDP" : "Serial only");

    // Mark complete only when UDP path was available so AgIO can see boot faults.
    if (net_connected) {
        s_startup_errors_pending = false;
    }
}

// ===================================================================
// Update dynamic hardware status (late detection)
// ===================================================================
void modulesUpdateStatus(void) {
    if (!feat::safety()) return;

    // Safety circuit: dynamic monitoring
    bool current_safety = hal_safety_ok();
    if (current_safety != s_hw.safety_ok) {
        s_hw.safety_ok = current_safety;
        if (!current_safety) {
            hal_log("MODULES: Safety circuit KICK detected!");
        } else {
            hal_log("MODULES: Safety circuit restored");
        }
    }
}

// ===================================================================
// Hardware Feature Module System — TASK-027
// ===================================================================
// Three-value module state with runtime activation, pin-claim
// arbitration, and dependency checking.
//
// The feature module table (g_features[]) is initialised with
// pin arrays and counts from the active board profile header
// (included via fw_config.h).  The 'compiled' flag is set by
// modulesInit() based on feature flags and pin availability.
// ===================================================================

/// Build a static owner tag string for pin claims (e.g. "MOD_IMU").
static const char* featureOwnerTag(FirmwareFeatureId id) {
    switch (id) {
        case MOD_IMU:    return "MOD_IMU";
        case MOD_ADS:    return "MOD_ADS";
        case MOD_ACT:    return "MOD_ACT";
        case MOD_ETH:    return "MOD_ETH";
        case MOD_GNSS:   return "MOD_GNSS";
        case MOD_NTRIP:  return "MOD_NTRIP";
        case MOD_SAFETY: return "MOD_SAFETY";
        case MOD_LOGSW:  return "MOD_LOGSW";
        case MOD_SD:     return "MOD_SD";
        default:         return "MOD_???";
    }
}

/// Feature module descriptor table.
/// Pin arrays and counts come from the board profile (included via fw_config.h).
/// compiled/hw_detected are set to false here; modulesInit() sets them properly.
static FeatureModuleInfo g_features[MOD_COUNT] = {
    /*  0 */ { "IMU",     MOD_UNAVAILABLE, false, false, FEAT_PINS_IMU,     FEAT_PINS_IMU_COUNT,   nullptr          },
    /*  1 */ { "ADS",     MOD_UNAVAILABLE, false, false, FEAT_PINS_ADS,     FEAT_PINS_ADS_COUNT,   nullptr          },
    /*  2 */ { "ACT",     MOD_UNAVAILABLE, false, false, FEAT_PINS_ACT,     FEAT_PINS_ACT_COUNT,   FEAT_DEPS_ACT    },
    /*  3 */ { "ETH",     MOD_UNAVAILABLE, false, false, FEAT_PINS_ETH,     FEAT_PINS_ETH_COUNT,   nullptr          },
    /*  4 */ { "GNSS",    MOD_UNAVAILABLE, false, false, FEAT_PINS_GNSS,    FEAT_PINS_GNSS_COUNT,  nullptr          },
    /*  5 */ { "NTRIP",   MOD_UNAVAILABLE, false, false, FEAT_PINS_NTRIP,   FEAT_PINS_NTRIP_COUNT, FEAT_DEPS_NTRIP  },
    /*  6 */ { "SAFETY",  MOD_UNAVAILABLE, false, false, FEAT_PINS_SAFETY,  FEAT_PINS_SAFETY_COUNT,nullptr          },
    /*  7 */ { "LOGSW",   MOD_UNAVAILABLE, false, false, FEAT_PINS_LOGSW,   FEAT_PINS_LOGSW_COUNT, nullptr          },
    /*  8 */ { "SD",      MOD_UNAVAILABLE, false, false, FEAT_PINS_SD,      FEAT_PINS_SD_COUNT,    nullptr          },
};

/// Set compiled flag and initial state for all feature modules.
/// Called from modulesInit().
static void featureModulesInitCompiled(void) {
    // IMU: needs FEAT_IMU feature flag AND at least one valid pin
    g_features[MOD_IMU].compiled = feat::imu() && (g_features[MOD_IMU].pin_count > 0);

    // ADS (steer angle sensor): needs FEAT_ADS AND valid pins
    g_features[MOD_ADS].compiled = feat::ads() && (g_features[MOD_ADS].pin_count > 0);

    // ACT (actuator): needs FEAT_ACT AND valid pins
    g_features[MOD_ACT].compiled = feat::act() && (g_features[MOD_ACT].pin_count > 0);

    // ETH: needs FEAT_ETH AND valid pins
    g_features[MOD_ETH].compiled = feat::eth() && (g_features[MOD_ETH].pin_count > 0);

    // GNSS: needs FEAT_GNSS AND valid pins
    g_features[MOD_GNSS].compiled = feat::gnss() && (g_features[MOD_GNSS].pin_count > 0);

    // NTRIP: needs FEAT_NTRIP (no dedicated pins)
    g_features[MOD_NTRIP].compiled = feat::ntrip();

    // SAFETY: needs FEAT_SAFETY AND valid pins
    g_features[MOD_SAFETY].compiled = feat::safety() && (g_features[MOD_SAFETY].pin_count > 0);

    // LOGSW: always available if pins are populated (no feature flag needed)
    g_features[MOD_LOGSW].compiled = (g_features[MOD_LOGSW].pin_count > 0);

    // SD: available if board exposes SD-related pins
    g_features[MOD_SD].compiled = (g_features[MOD_SD].pin_count > 0);

    // Set initial state: use cfg:: defaults for specific modules,
    // MOD_OFF for all other compiled modules, MOD_UNAVAILABLE for uncompiled.
    for (uint8_t i = 0; i < MOD_COUNT; i++) {
        if (!g_features[i].compiled) {
            g_features[i].state = MOD_UNAVAILABLE;
            continue;
        }
        if (i == MOD_NTRIP) {
            g_features[i].state = static_cast<int8_t>(cfg::MOD_DEFAULT_NTRIP);
        } else if (i == MOD_LOGSW) {
            g_features[i].state = static_cast<int8_t>(cfg::MOD_DEFAULT_LOGSW);
        } else if (i == MOD_SD) {
            g_features[i].state = static_cast<int8_t>(cfg::MOD_DEFAULT_SD);
        } else {
            g_features[i].state = MOD_OFF;
        }
    }

    // Log feature module availability summary
    hal_log("FEAT-MOD: === Feature Module Availability ===");
    for (uint8_t i = 0; i < MOD_COUNT; i++) {
        const char* state_str = "???";
        switch (static_cast<ModState>(g_features[i].state)) {
            case MOD_UNAVAILABLE: state_str = "UNAVAIL"; break;
            case MOD_OFF:         state_str = "OFF";     break;
            case MOD_ON:          state_str = "ON";      break;
        }
        hal_log("FEAT-MOD:   [%u] %-8s compiled=%s pins=%u state=%s",
                (unsigned)i,
                g_features[i].name,
                g_features[i].compiled ? "Y" : "N",
                (unsigned)g_features[i].pin_count,
                state_str);
    }
}

/// Update feature module hw_detected from existing hardware detection results.
/// Called from modulesInit() after hardware detection.
static void featureModulesSyncHwDetected(void) {
    g_features[MOD_IMU].hw_detected    = s_hw.imu_detected;
    g_features[MOD_ADS].hw_detected    = s_hw.was_detected;
    g_features[MOD_ACT].hw_detected    = s_hw.actuator_detected;
    g_features[MOD_ETH].hw_detected    = s_hw.eth_detected;
    g_features[MOD_SAFETY].hw_detected = s_hw.safety_ok;
    g_features[MOD_SD].hw_detected     = s_hw.sd_present;
    // GNSS and NTRIP hw_detected are set by their respective subsystems
    // (not tracked in ModuleHwStatus yet).
    // LOGSW has no hardware detection (simple GPIO switch).
}

// --- Feature Module Public API ---

bool moduleActivate(FirmwareFeatureId id) {
    if (id >= MOD_COUNT) return false;

    FeatureModuleInfo& feat = g_features[id];

    // 1. Check compiled
    if (!feat.compiled) {
        hal_log("FEAT-MOD: activate(%s) rejected: not compiled", feat.name);
        return false;
    }

    // 2. Already active?
    if (feat.state == MOD_ON) {
        return true;  // idempotent
    }

    // 3. Check dependencies
    //    Terminator is 0xFF — no valid FirmwareFeatureId can equal 255.
    if (feat.deps != nullptr) {
        for (uint8_t d = 0; feat.deps[d] != 0xFF; d++) {
            FirmwareFeatureId dep_id = static_cast<FirmwareFeatureId>(feat.deps[d]);
            if (dep_id >= MOD_COUNT) continue;
            if (g_features[dep_id].state != MOD_ON) {
                hal_log("FEAT-MOD: activate(%s) rejected: dep %s not active",
                        feat.name, g_features[dep_id].name);
                return false;
            }
        }
    }

    // 4. Claim pins (hard conflict detection per ADR-HAL-001).
    //    A pin claimed by a different owner is a hard conflict — the
    //    activation fails with rollback.  Same-owner re-claim is OK
    //    (idempotent moduleActivate).
    const char* owner = featureOwnerTag(id);
    uint8_t new_claims = 0;
    for (uint8_t p = 0; p < feat.pin_count; p++) {
        const int8_t pin = feat.pins[p];
        if (pin < 0) continue;

        const char* existing_owner = hal_pin_claim_owner(pin);
        if (existing_owner) {
            if (std::strcmp(existing_owner, owner) == 0) {
                // Same owner — idempotent, skip.
            } else {
                // Different owner — hard conflict (ADR-HAL-001).
                hal_log("FEAT-MOD: activate(%s) CONFLICT on GPIO %d (%s vs %s)",
                        feat.name, (int)pin, existing_owner, owner);
                hal_pin_claim_release(owner);
                return false;
            }
        } else {
            // Pin not yet claimed — claim it for this module.
            if (!hal_pin_claim_add(pin, owner)) {
                hal_log("FEAT-MOD: activate(%s) pin claim failed for GPIO %d",
                        feat.name, (int)pin);
                hal_pin_claim_release(owner);
                return false;
            }
            new_claims++;
        }
    }

    // 5. Set state to MOD_ON
    feat.state = MOD_ON;
    hal_log("FEAT-MOD: activate(%s) -> ON (total_pins=%u, new_claims=%u)",
            feat.name, (unsigned)feat.pin_count, (unsigned)new_claims);
    return true;
}

bool moduleDeactivate(FirmwareFeatureId id) {
    if (id >= MOD_COUNT) return false;

    FeatureModuleInfo& feat = g_features[id];

    if (feat.state != MOD_ON) {
        return true;  // idempotent
    }

    const char* owner = featureOwnerTag(id);
    int released = hal_pin_claim_release(owner);

    feat.state = MOD_OFF;
    hal_log("FEAT-MOD: deactivate(%s) -> OFF (released %d pins)",
            feat.name, released);
    return true;
}

bool moduleIsActive(FirmwareFeatureId id) {
    if (id >= MOD_COUNT) return false;
    return g_features[id].state == MOD_ON;
}

bool moduleControlPipelineReady(char* reason_buf, size_t reason_buf_len) {
    auto set_reason = [&](const char* msg) {
        if (!reason_buf || reason_buf_len == 0) return;
        std::snprintf(reason_buf, reason_buf_len, "%s", msg);
    };

    if (!moduleIsActive(MOD_IMU)) {
        set_reason("MOD_IMU inactive");
        return false;
    }
    if (!moduleIsActive(MOD_ADS)) {
        set_reason("MOD_ADS inactive");
        return false;
    }
    if (!moduleIsActive(MOD_ACT)) {
        set_reason("MOD_ACT inactive");
        return false;
    }

    set_reason("OK");
    return true;
}

ModState moduleGetState(FirmwareFeatureId id) {
    if (id >= MOD_COUNT) return MOD_UNAVAILABLE;
    return static_cast<ModState>(g_features[id].state);
}

const FeatureModuleInfo* moduleGetInfo(FirmwareFeatureId id) {
    if (id >= MOD_COUNT) return nullptr;
    return &g_features[id];
}
