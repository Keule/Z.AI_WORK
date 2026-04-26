/**
 * @file main.cpp
 * @brief ESP32-S3 firmware main entry point.
 *
 * Target: LilyGO T-ETH-Lite-S3 (ESP32-S3-WROOM-1 + W5500 Ethernet)
 *
 * Two FreeRTOS tasks (ADR-007):
 *   - task_fast (Core 1): Configurable Hz module pipeline (input → process → output)
 *   - task_slow (Core 0): HW monitoring, DBG.loop(), CLI, WDT,
 *                        SD flush, NTRIP connect/reconnect, ETH monitoring
 *
 * Operating Modes (OpMode): CONFIG and WORK.
 *   - CONFIG: Safety LOW — configuration mode, pipeline paused
 *   - WORK:   Safety HIGH — autosteer operation, pipeline active
 *
 * NOTE: Hardware init is done in setup() via hal_esp32_init_all().
 *       Tasks do NOT re-initialize anything.
 *
 * Boot sequence (ADR-MODULE-002 refactoring):
 *   bootInitHal()          — NVS, HAL, firmware version
 *   bootInitModules()      — moduleSysInit, moduleSysBootActivate, SD OTA check
 *   bootInitDebugConsole() — DebugConsole TCP/Telnet server (early, after ETH init)
 *   bootInitConfig()       — Config framework, soft config, NVS check
 *   bootInitCommunication()— CLI, NTRIP, UM980 setup
 *   bootEnterMode()        — OpMode decision (CONFIG/WORK), safety check
 *   bootInitControl()      — Pipeline check, calibration, HW status
 *   bootStartTasks()       — Startup errors, FreeRTOS tasks (task_fast + task_slow)
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <cstdio>
#include <cstring>

#include "fw_config.h"
#include "hal/hal.h"
#include "hal_esp32/hal_impl.h"
#include "logic/dependency_policy.h"
#include "logic/mod_network.h"
#include "logic/mod_steer.h"
#include "logic/features.h"
#include "logic/global_state.h"
#include "logic/hw_status.h"
#include "logic/module_interface.h"
#include "logic/ntrip.h"
#include "logic/nvs_config.h"
#include "logic/runtime_config.h"
#include "logic/sd_ota.h"
#include "logic/sd_logger.h"
#include "logic/cli.h"
#include "logic/setup_wizard.h"
#include "logic/um980_uart_setup.h"
#include "logic/config_framework.h"
#include "logic/config_ntrip.h"
#include "logic/config_network.h"
#include "logic/config_gnss.h"
#include "logic/config_system.h"
#include "logic/config_pid.h"
#include "logic/config_actuator.h"
#include "logic/config_menu.h"
#include "debug/DebugConsole.h"

#include "logic/log_config.h"
#undef LOG_LOCAL_LEVEL          // Arduino.h already defined it via esp_log.h
#define LOG_LOCAL_LEVEL LOG_LEVEL_MAIN
#include "esp_log.h"
#include "logic/log_ext.h"

#if defined(__has_include) && __has_include(<BluetoothSerial.h>) && \
    (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3) || \
     (defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3) && \
      !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)))
#include <BluetoothSerial.h>
#define MAIN_BT_SPP_AVAILABLE 1
#else
#define MAIN_BT_SPP_AVAILABLE 0
#endif

// ===================================================================
// Task handles (ADR-007: two-task architecture)
// ===================================================================
static TaskHandle_t s_task_fast_handle = nullptr;
static TaskHandle_t s_task_slow_handle = nullptr;

// Forward declarations
static void taskFastFunc(void* param);
static void taskSlowFunc(void* param);

// ===================================================================
// task_fast frequency configuration (ADR-007)
// ===================================================================
#ifndef TASK_FAST_HZ
#define TASK_FAST_HZ 100
#endif
static constexpr uint32_t TASK_FAST_INTERVAL_MS = 1000 / TASK_FAST_HZ;

// ===================================================================
// Runtime/Debug logging knobs
// ===================================================================
static constexpr bool MAIN_VERBOSE_TASK_DBG = false;  // true => print loop Hz heartbeats
static constexpr uint32_t MAIN_HW_ERR_REMINDER_MS = 30000;
static constexpr size_t MAIN_BOOT_CLI_BUF_CAP = 128;
static constexpr char MAIN_BOOT_AP_SSID[] = "AgSteer-Boot";
static constexpr char MAIN_BOOT_AP_PASS[] = "agsteer123";
static const IPAddress MAIN_BOOT_AP_IP(192, 168, 4, 1);
static const IPAddress MAIN_BOOT_AP_GW(192, 168, 4, 1);
static const IPAddress MAIN_BOOT_AP_MASK(255, 255, 255, 0);
static WebServer s_boot_web_server(80);
static bool s_boot_web_ota_active = false;
static bool s_boot_ap_active = false;
static bool s_boot_eth_url_logged = false;
#if MAIN_BT_SPP_AVAILABLE
static BluetoothSerial s_boot_bt_serial;
static bool s_boot_bt_active = false;
#endif

// Forward declarations (defined in loop section, used by TCP input callback)
static uint32_t s_cli_last_rx_ms = 0;

// ===================================================================
// Shared boot state (valid during setup() only)
// ===================================================================
static bool s_control_pipeline_ready = false;

// ===================================================================
// NVS flash init helper
// ===================================================================
static void initNvsFlashStorage(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_OK) {
        return;
    }

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        hal_log("BOOT: NVS init returned %s -> erasing NVS partition",
                esp_err_to_name(err));
        const esp_err_t erase_err = nvs_flash_erase();
        if (erase_err != ESP_OK) {
            hal_log("BOOT: NVS erase failed: %s", esp_err_to_name(erase_err));
            return;
        }

        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        hal_log("BOOT: NVS init failed: %s", esp_err_to_name(err));
    }
}

// ===================================================================
// Utility helpers
// ===================================================================
static inline bool shouldLogPeriodic(uint32_t now_ms, uint32_t* last_ms, uint32_t interval_ms) {
    if (now_ms - *last_ms < interval_ms) return false;
    *last_ms = now_ms;
    return true;
}

static void formatIpU32(uint32_t ip, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    std::snprintf(out, out_sz, "%u.%u.%u.%u",
                  static_cast<unsigned>((ip >> 24) & 0xFF),
                  static_cast<unsigned>((ip >> 16) & 0xFF),
                  static_cast<unsigned>((ip >> 8) & 0xFF),
                  static_cast<unsigned>(ip & 0xFF));
}

// ===================================================================
// Boot Maintenance helpers (CLI session, Web OTA, BT SPP)
// ===================================================================

static void bootMaintRunCliSession(void) {
    DBG.println();
    DBG.println("=== Boot CLI ===");
    DBG.println("System init complete. Type commands now.");
    DBG.println("Type 'boot' or 'exit' to continue startup.");

    char line_buf[MAIN_BOOT_CLI_BUF_CAP];
    size_t line_len = 0;

    while (true) {
        // Process USB Serial input — echo goes through DBG (Serial + TCP)
        {
            while (Serial.available()) {
                const int ch = Serial.read();
                if (ch == '\r' || ch == '\n') {
                    if (line_len == 0) continue;
                    line_buf[line_len] = '\0';
                    DBG.println();
                    if (std::strcmp(line_buf, "boot") == 0 || std::strcmp(line_buf, "exit") == 0) {
                        DBG.println("Leaving Boot CLI, continuing startup...");
                        return;
                    }
                    cliProcessLine(line_buf);  // s_cli_out is already &DBG
                    line_len = 0;
                } else if (ch == 3) {  // Ctrl+C
                    line_len = 0;
                    DBG.println("^C");
                } else if (ch == 8 || ch == 127) {  // Backspace / DEL
                    if (line_len > 0) {
                        line_len--;
                        DBG.print("\b \b");
                    }
                } else if (line_len + 1 < sizeof(line_buf)) {
                    line_buf[line_len++] = static_cast<char>(ch);
                    DBG.print(static_cast<char>(ch));
                }
            }
        }
#if MAIN_BT_SPP_AVAILABLE
        // Process BT SPP input — echo also goes through DBG (Serial + TCP)
        if (s_boot_bt_active) {
            while (s_boot_bt_serial.available()) {
                const int ch = s_boot_bt_serial.read();
                if (ch == '\r' || ch == '\n') {
                    if (line_len == 0) continue;
                    line_buf[line_len] = '\0';
                    DBG.println();
                    if (std::strcmp(line_buf, "boot") == 0 || std::strcmp(line_buf, "exit") == 0) {
                        DBG.println("Leaving Boot CLI, continuing startup...");
                        return;
                    }
                    cliProcessLine(line_buf);
                    line_len = 0;
                } else if (ch == 3) {
                    line_len = 0;
                    DBG.println("^C");
                } else if (ch == 8 || ch == 127) {
                    if (line_len > 0) {
                        line_len--;
                        DBG.print("\b \b");
                    }
                } else if (line_len + 1 < sizeof(line_buf)) {
                    line_buf[line_len++] = static_cast<char>(ch);
                    DBG.print(static_cast<char>(ch));
                }
            }
        }
#endif

        // Debug Console: TCP accept/input/disconnect (non-blocking)
        // CRITICAL: Without this, remote TCP connections are never accepted
        // during the boot CLI session, and no output/input reaches the client.
        DBG.loop();

        if (s_boot_web_ota_active) {
            s_boot_web_server.handleClient();
            if (!s_boot_eth_url_logged && hal_net_is_connected()) {
                char ip_buf[20] = {0};
                formatIpU32(hal_net_get_ip(), ip_buf, sizeof(ip_buf));
                hal_log("BOOT: Web OTA also via ETH: http://%s/", ip_buf);
                s_boot_eth_url_logged = true;
            }
        }
        // NOTE: NTRIP tick/read/forward is handled by task_slow.
        // Do NOT call ntripTick() here — it blocks up to 5 s in hal_tcp_connect()
        // and would starve the IDLE task / trigger WDT.
        um980SetupConsoleTick();
        delay(10);
    }
}

static void bootMaintWebHandleRoot(void) {
    static const char kPage[] =
        "<!doctype html><html><head><meta charset='utf-8'><title>AgSteer OTA</title></head>"
        "<body><h2>AgSteer Boot OTA</h2>"
        "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<input type='file' name='firmware' accept='.bin' required>"
        "<button type='submit'>Upload Firmware</button></form>"
        "<p>Nach erfolgreichem Upload startet das Geraet neu.</p>"
        "</body></html>";
    s_boot_web_server.send(200, "text/html", kPage);
}

static void bootMaintWebHandleUpdateDone(void) {
    const bool ok = !Update.hasError();
    s_boot_web_server.send(200, "text/plain", ok ? "OK - rebooting" : "FAIL");
    if (ok) {
        hal_log("BOOT: Web OTA successful -> reboot");
        delay(500);
        ESP.restart();
    }
}

static void bootMaintWebHandleUpdateUpload(void) {
    HTTPUpload& upload = s_boot_web_server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        hal_log("BOOT: Web OTA upload start: %s", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            hal_log("BOOT: Web OTA Update.begin failed");
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        const size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            hal_log("BOOT: Web OTA write failed (%u/%u)",
                    (unsigned)written,
                    (unsigned)upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            hal_log("BOOT: Web OTA upload complete (%u bytes)", (unsigned)upload.totalSize);
        } else {
            hal_log("BOOT: Web OTA Update.end failed");
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        hal_log("BOOT: Web OTA upload aborted");
    }
}

static void bootMaintStartServices(void) {
    const bool eth_up = hal_net_is_connected();

    // Web OTA Server (ueber Ethernet erreichbar, unabhaengig von WiFi)
    s_boot_web_server.on("/", HTTP_GET, bootMaintWebHandleRoot);
    s_boot_web_server.on("/update", HTTP_POST, bootMaintWebHandleUpdateDone, bootMaintWebHandleUpdateUpload);
    s_boot_web_server.begin();
    s_boot_web_ota_active = true;

    if (eth_up) {
        // Ethernet ist verfuegbar — WiFi AP und BT SPP ueberspringen
        char ip_buf[20] = {0};
        formatIpU32(hal_net_get_ip(), ip_buf, sizeof(ip_buf));
        hal_log("BOOT: ETH connected — WiFi/BT skipped (Web OTA via ETH: http://%s/)", ip_buf);
        s_boot_eth_url_logged = true;
        um980SetupSetConsoleMirror(nullptr);
        return;
    }

    // Kein Ethernet — WiFi AP + BT SPP starten (Fallback fuer Boot-Konfiguration)
    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAPConfig(MAIN_BOOT_AP_IP, MAIN_BOOT_AP_GW, MAIN_BOOT_AP_MASK)) {
        hal_log("BOOT: WiFi AP config failed, using stack defaults");
    }

    s_boot_ap_active = WiFi.softAP(MAIN_BOOT_AP_SSID, MAIN_BOOT_AP_PASS, 1, 0, 2);
    if (!s_boot_ap_active) {
        hal_log("BOOT: WiFi AP WPA2 start failed -> fallback OPEN AP");
        s_boot_ap_active = WiFi.softAP(MAIN_BOOT_AP_SSID, nullptr, 1, 0, 2);
    }
    if (s_boot_ap_active) {
        IPAddress ip = WiFi.softAPIP();
        hal_log("BOOT: WiFi AP active SSID=%s IP=%s CH=%u",
                MAIN_BOOT_AP_SSID,
                ip.toString().c_str(),
                (unsigned)WiFi.channel());
    } else {
        hal_log("BOOT: WiFi AP start failed (SSID=%s)", MAIN_BOOT_AP_SSID);
    }

    hal_log("BOOT: Web OTA active at http://%s/", WiFi.softAPIP().toString().c_str());
    s_boot_eth_url_logged = false;
    hal_log("BOOT: ETH link/IP not ready yet (Web OTA URL will be logged when available)");

#if MAIN_BT_SPP_AVAILABLE
    s_boot_bt_active = s_boot_bt_serial.begin("AgSteer-BootCLI");
    hal_log("BOOT: BT SPP %s", s_boot_bt_active ? "active" : "start failed");
    um980SetupSetConsoleMirror(s_boot_bt_active ? static_cast<Stream*>(&s_boot_bt_serial) : nullptr);
#else
    hal_log("BOOT: BT SPP unavailable on this target");
    um980SetupSetConsoleMirror(nullptr);
#endif
}

static void bootMaintStopServices(void) {
#if MAIN_BT_SPP_AVAILABLE
    if (s_boot_bt_active) {
        s_boot_bt_serial.end();
        s_boot_bt_active = false;
    }
#endif
    um980SetupSetConsoleMirror(nullptr);

    if (s_boot_web_ota_active) {
        s_boot_web_server.stop();
        s_boot_web_ota_active = false;
    }
    s_boot_eth_url_logged = false;
    if (s_boot_ap_active) {
        WiFi.softAPdisconnect(true);
        s_boot_ap_active = false;
    }
    WiFi.mode(WIFI_OFF);
}

// ===================================================================
// Boot Phase 1: HAL & Hardware Init
//
// Initialisiert: NVS Flash, HAL (SPI, ETH, GPIO, Sensor-Bus),
//                Firmware-Version
//
// NOTE: imuInit(), wasInit(), actuatorInit() are NO LONGER called here.
//       Module activation (moduleSysBootActivate) handles their init
//       via mod_imu.activate(), mod_was.activate(), mod_actuator.activate().
// ===================================================================
static void bootInitHal(void) {
    uint32_t t_phase = hal_millis();

    // NVS Flash initialisieren
    initNvsFlashStorage();

    // HAL: SPI-Bus, W5500 Ethernet, GPIO, Safety-Pin, Sensor-Bus
    hal_esp32_init_all();
    hal_log("BOOT: hal_esp32_init_all ... %lu ms", (unsigned long)(hal_millis() - t_phase));

    // Firmware-Version & Build-Info (immer ausgeben)
    {
        SdOtaVersion ver = sdOtaGetCurrentVersion();
        const esp_partition_t* part = esp_ota_get_running_partition();
        hal_log("========================================");
        hal_log("  AgSteer Firmware v%u.%u.%u", ver.major, ver.minor, ver.patch);
        hal_log("  Build: %s %s", __DATE__, __TIME__);
        hal_log("  Partition: %s (0x%06X)", part ? part->label : "?",
                part ? (unsigned)part->address : 0);
        hal_log("  Flash: %d KB free", (int)(ESP.getFreeSketchSpace() / 1024));
        hal_log("========================================");
    }
}

// ===================================================================
// Boot Phase 2: Module System (ADR-MODULE-002)
//
// Initialisiert: Unified module registry, boot activation,
//                SD-Card OTA Check
// ===================================================================
static void bootInitModules(void) {
    uint32_t t_phase = hal_millis();

    // Unified module system init — registers all 13 modules
    moduleSysInit();

    // Boot activation — activates modules in dependency order:
    // ETH first (transport), then sensors, then services, then steer last
    moduleSysBootActivate();
    hal_log("BOOT: module system init ... %lu ms", (unsigned long)(hal_millis() - t_phase));

    // SD-Card OTA Firmware Update (if LOGGING module is active and detected SD)
    if (moduleSysIsActive(ModuleId::LOGGING)) {
        const auto* log_mod = moduleSysGet(ModuleId::LOGGING);
        if (log_mod && log_mod->state.detected) {
            if (isFirmwareUpdateAvailableOnSD()) {
                hal_log("Main: firmware update detected on SD card – starting update");
                updateFirmwareFromSD();
                hal_log("Main: OTA update FAILED, continuing with current firmware");
            }
        }
    }
}

// ===================================================================
// Boot Phase 2.5: Debug Console (TCP/Telnet Server)
//
// Starts the DebugConsole as early as possible after module system init
// (which activates ETH + DHCP). The TCP server begins listening
// immediately — once ETH gets an IP address, remote connections work.
// DBG already writes to Serial by default (fallback when no TCP client).
// ===================================================================
static void bootInitDebugConsole(void) {
    uint32_t t_phase = hal_millis();

    // Debug Console: TCP/Telnet server (fan-out to Serial + TCP)
    // DBG is already writing to Serial since construction.
    // begin() starts the TCP server; enableTcp(true) activates it.
    DBG.begin(23);              // TCP port 23 (telnet)
    DBG.enableTcp(true);
    DBG.setInputCallback([](uint8_t c) {
        // TCP client input → same CLI processing as Serial input
        static char s_tcp_buf[128];
        static size_t s_tcp_len = 0;

        s_cli_last_rx_ms = hal_millis();
        if (c == '\r' || c == '\n') {
            if (s_tcp_len > 0) {
                s_tcp_buf[s_tcp_len] = '\0';
                DBG.println();  // Echo newline to both consoles
                cliProcessLine(s_tcp_buf);
                s_tcp_len = 0;
            }
        } else if (c == 3) {  // Ctrl+C
            s_tcp_len = 0;
            DBG.println("^C");
        } else if (c == 8 || c == 127) {  // Backspace / DEL
            if (s_tcp_len > 0) {
                s_tcp_len--;
                DBG.print("\b \b");
            }
        } else if (s_tcp_len + 1 < sizeof(s_tcp_buf)) {
            s_tcp_buf[s_tcp_len++] = static_cast<char>(c);
            DBG.print(static_cast<char>(c));  // Echo to both consoles
        }
    });
    // s_cli_out is already &DBG (set in cli.cpp default), so all CLI
    // command output now automatically goes to Serial + TCP.
    {
        char ip_buf[20] = {0};
        if (hal_net_is_connected()) {
            formatIpU32(hal_net_get_ip(), ip_buf, sizeof(ip_buf));
            hal_log("DBG: TCP console listening on %s:23 (telnet/nc)", ip_buf);
        } else {
            hal_log("DBG: TCP console listening on port 23 (waiting for network...)");
        }
    }

    hal_log("BOOT: debug console init ... %lu ms", (unsigned long)(hal_millis() - t_phase));
}

// ===================================================================
// Boot Phase 3: Configuration
//
// Initialisiert: Config Framework, Runtime-Config (NVS, SD),
//                Setup-Wizard Pending-Check
// ===================================================================
static void bootInitConfig(void) {
    uint32_t t_phase = hal_millis();

    // Config Framework (ADR-006, TASK-037)
    configFrameworkInit();
    configNetworkInit();
    configNtripInit();
    configGnssInit();
    configPidInit();
    configActuatorInit();
    configSystemInit();
    configMenuInit();
    hal_log("BOOT: config framework init ... %lu ms", (unsigned long)(hal_millis() - t_phase));

    // Soft Config: Defaults → NVS → SD Overrides (TASK-028)
    t_phase = hal_millis();
    softConfigLoadDefaults(softConfigGet());
    nvsConfigLoad(softConfigGet());
    if (moduleSysIsActive(ModuleId::LOGGING)) {
        softConfigLoadOverrides(softConfigGet());  // TASK-033: /ntrip.cfg von SD
    } else {
        hal_log("Main: LOGGING module inactive -> skip SD runtime config overrides");
    }
    hal_log("BOOT: config load ... %lu ms", (unsigned long)(hal_millis() - t_phase));

    // Setup-Wizard falls keine NVS-Config vorhanden
    if (!nvsConfigHasData()) {
        hal_log("Main: no NVS config found -> setup wizard pending");
        setupWizardRequestStart();
    }
}

// ===================================================================
// Boot Phase 4: Communication Init
//
// Initialisiert: CLI, NTRIP Client, UM980 UART Setup
//
// NOTE: ntripInit()/ntripSetConfig() still called here to initialize
//       the NTRIP state machine (even though mod_ntrip.activate() also
//       calls ntripInit() — idempotent).
// ===================================================================
static void bootInitCommunication(void) {
    // Serial CLI initialisieren
    cliInit();

    // Debug Console TCP server already initialized in bootInitDebugConsole().
    // Input callback already set there.  Nothing to do here.

    // NTRIP Client (conditional compile)
#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
    ntripInit();
    {
        RuntimeConfig& rcfg = softConfigGet();
        ntripSetConfig(
            rcfg.ntrip_host,
            rcfg.ntrip_port,
            rcfg.ntrip_mountpoint,
            rcfg.ntrip_user,
            rcfg.ntrip_password
        );
        if (rcfg.ntrip_host[0] == '\0' || rcfg.ntrip_mountpoint[0] == '\0') {
            hal_log("Main: NTRIP not configured (host or mountpoint empty) — skipping");
        } else {
            hal_log("Main: NTRIP client configured (host=%s, port=%u, mp=%s)",
                    g_ntrip_config.host,
                    static_cast<unsigned>(g_ntrip_config.port),
                    g_ntrip_config.mountpoint);
        }
    }
#endif

    // UM980 GNSS UART Setup
    {
        RuntimeConfig& rcfg = softConfigGet();
        // Per-Port UART Baud und Rollen aus RuntimeConfig (NVS) laden
        um980SetupSetBaud(0, rcfg.gnss_uart_a_baud);
        um980SetupSetBaud(1, rcfg.gnss_uart_b_baud);
        um980SetupSetRole(0, rcfg.gnss_uart_a_role);
        um980SetupSetRole(1, rcfg.gnss_uart_b_role);
        um980SetupApply();
    }
}

// ===================================================================
// Boot Phase 5: Operating Mode Decision (ADR-MODULE-002)
//
// Liest Safety-Pin und entscheidet: CONFIG oder WORK mode.
// Uses the OpMode enum (CONFIG/WORK) from module_interface.h (ADR-007).
// ===================================================================
static void bootEnterMode(void) {
    uint32_t t_phase = hal_millis();

    const bool boot_safety_low = !hal_safety_ok();

    if (boot_safety_low) {
        hal_log("BOOT: safety LOW → CONFIG mode");
        // Safety LOW: offer boot CLI for configuration
        bootMaintStartServices();
        bootMaintRunCliSession();
        bootMaintStopServices();
        // Stay in CONFIG mode (modeGet() defaults to CONFIG)
    } else {
        // Safety HIGH: try to enter WORK mode
        if (modeSet(OpMode::WORK)) {
            hal_log("BOOT: safety HIGH → WORK mode");
        } else {
            hal_log("BOOT: safety HIGH but WORK mode rejected (pipeline incomplete)");
            // Stay in CONFIG mode
        }
    }

    hal_log("BOOT: mode decision ... %lu ms (mode=%s)",
            (unsigned long)(hal_millis() - t_phase),
            modeToString(modeGet()));
}

// ===================================================================
// Boot Phase 6: Control System Init
//
// Prueft Pipeline-Readiness, Kalibrierung, HW-Status.
//
// NOTE: controlInit() is NO LONGER called here — mod_steer.activate()
//       already calls pidInit() + controlInit().
// ===================================================================
static void bootInitControl(void) {
    uint32_t t_phase = hal_millis();

    // Control-Pipeline Pruefung via module system
    s_control_pipeline_ready =
        moduleSysIsActive(ModuleId::IMU) &&
        moduleSysIsActive(ModuleId::WAS) &&
        moduleSysIsActive(ModuleId::ACTUATOR) &&
        moduleSysIsActive(ModuleId::SAFETY) &&
        moduleSysIsActive(ModuleId::STEER);

    if ((feat::act() && feat::safety()) && s_control_pipeline_ready) {
        hal_log("Main: control pipeline ready (IMU+WAS+ACT+SAFETY+STEER)");
    } else if (feat::act() && feat::safety()) {
        hal_log("Main: control pipeline NOT ready");
    } else {
        hal_log("Main: control loop feature disabled");
    }
    hal_log("BOOT: control init ... %lu ms", (unsigned long)(hal_millis() - t_phase));

    // Lenkwinkel-Kalibrierung
    // Bei erstem Boot (keine Kalibrierung in NVS): automatische Kalibrierung.
    // Bei folgenden Boots: aus NVS laden, 'c' innerhalb 3s fuer Neukalibrierung.
    t_phase = hal_millis();
    if (feat::ads()) {
        bool need_cal = !hal_steer_angle_is_calibrated();

        if (!need_cal) {
            // Bereits kalibriert — 3s Fenster fuer Neukalibrierung
            uint32_t cal_wait_start = millis();
            DBG.println();
            DBG.println("Druecke 'c' + ENTER fuer Neukalibrierung (3s)...");
            DBG.flush();
            while (millis() - cal_wait_start < 3000) {
                if (Serial.available()) {
                    int c = Serial.read();
                    if (c == 'c' || c == 'C') {
                        need_cal = true;
                        while (Serial.available()) Serial.read();
                        break;
                    }
                }
                delay(10);
            }
        }

        if (need_cal) {
            hal_log("Main: %s calibration",
                    hal_steer_angle_is_calibrated() ? "forced re" : "initial");
            hal_steer_angle_calibrate();
        } else {
            hal_log("Main: calibration OK (loaded from NVS)");
        }
    } else {
        hal_log("Main: steer angle calibration skipped (sensor feature disabled)");
    }
    hal_log("BOOT: calibration ... %lu ms", (unsigned long)(hal_millis() - t_phase));

    // Hardware-Status Monitoring initialisieren
    hwStatusInit();
}

// ===================================================================
// Boot Phase 7: Start FreeRTOS Tasks (ADR-007)
//
// Erstellt: task_fast (Core 1), task_slow (Core 0)
// NOTE: SD/NTRIP/ETH are handled directly by task_slow (no sub-tasks).
//       PSRAM buffer allocation happens during module activation
//       (mod_logging.activate() calls sdLoggerInitPsram()).
// ===================================================================
static void bootStartTasks(void) {
    // -----------------------------------------------------------------
    // PSRAM buffer allocation (TASK-029)
    // -----------------------------------------------------------------
    // mod_logging.activate() already calls sdLoggerMaintInit() ->
    // sdLoggerInitPsram() if SD is detected.  If only NTRIP is active
    // (no SD), we still need to ensure PSRAM is initialised for the
    // ring buffer.  sdLoggerInitPsram() is idempotent (Issue 3 guard).
    // -----------------------------------------------------------------
    const bool logging_active = moduleSysIsActive(ModuleId::LOGGING);
    const auto* log_mod = moduleSysGet(ModuleId::LOGGING);
    const bool sd_detected = logging_active && log_mod && log_mod->state.detected;

    if (!sd_detected) {
        // No SD detected — ensure PSRAM buffer is still allocated for
        // the ring buffer (used by NTRIP and SD logging).
        sdLoggerInitPsram();
    }
    // else: LOGGING module already called sdLoggerInitPsram() during activation.

    // Startup-Errors melden (UDP wenn Netz up, sonst Serial)
    hal_delay_ms(100);
    mod_network_send_startup_errors();

    // task_fast auf Core 1 — module pipeline at configurable Hz (ADR-007)
    if ((feat::act() && feat::safety()) && s_control_pipeline_ready) {
        xTaskCreatePinnedToCore(
            taskFastFunc,
            "fast",
            4096,
            nullptr,
            configMAX_PRIORITIES - 2,  // hohe Prioritaet
            &s_task_fast_handle,
            1   // Core 1
        );
        hal_log("Main: task_fast created (%u Hz, Core 1)", (unsigned)TASK_FAST_HZ);
    } else {
        if (!(feat::act() && feat::safety())) {
            hal_log("Main: task_fast not started (feature disabled)");
        } else {
            hal_log("Main: task_fast not started (pipeline inactive)");
        }
    }

    // task_slow auf Core 0 — HW monitoring, DBG.loop(), CLI, WDT (ADR-007)
    xTaskCreatePinnedToCore(
        taskSlowFunc,
        "slow",
        8192,  // groesserer Stack fuer CLI/Config-Menü
        nullptr,
        configMAX_PRIORITIES - 4,  // mittel Prioritaet
        &s_task_slow_handle,
        0   // Core 0
    );
    hal_log("Main: task_slow created (Core 0)");
}

// ===================================================================
// task_fast – configurable Hz module pipeline on Core 1 (ADR-007)
//
// Executes the unified module pipeline: input → process → output
// Only runs when mode == WORK. Never blocks.
// ===================================================================
static void taskFastFunc(void* param) {
    (void)param;
    hal_log("task_fast: started on core %d (%u Hz)", xPortGetCoreID(), (unsigned)TASK_FAST_HZ);

    // Wait for network + sensors to stabilise
    vTaskDelay(pdMS_TO_TICKS(500));

    const TickType_t interval = pdMS_TO_TICKS(TASK_FAST_INTERVAL_MS);
    TickType_t next_wake = xTaskGetTickCount();
    uint32_t fast_dbg_count = 0;
    uint32_t fast_freq_start = hal_millis();
#if FEAT_ENABLED(FEAT_COMPILED_IMU) || FEAT_ENABLED(FEAT_COMPILED_ADS) || FEAT_ENABLED(FEAT_COMPILED_ACT)
    uint32_t last_spi_tm_ms = 0;
#endif

    for (;;) {
        // ADR-007: run module pipeline only in WORK mode
        if (modeGet() == OpMode::WORK) {
            const uint32_t now_ms = hal_millis();

            // === Module Pipeline ===
            moduleSysRunInput(now_ms);
            moduleSysRunProcess(now_ms);
            moduleSysRunOutput(now_ms);
        }

        if (MAIN_VERBOSE_TASK_DBG) {
            // Heartbeat DBG every 1s
            fast_dbg_count++;
            if (fast_dbg_count >= TASK_FAST_HZ) {
                uint32_t freq_now = hal_millis();
                float hz = (fast_dbg_count * 1000.0f) / (float)(freq_now - fast_freq_start);
                fast_freq_start = freq_now;
                fast_dbg_count = 0;
                DBG.printf("[DBG-FAST] %.1f Hz\r\n", hz);
            }
        }

        const uint32_t now_ms = hal_millis();
#if FEAT_ENABLED(FEAT_COMPILED_IMU) || FEAT_ENABLED(FEAT_COMPILED_ADS) || FEAT_ENABLED(FEAT_COMPILED_ACT)
        if (LOG_SPI_TIMING_INTERVAL_MS > 0 && now_ms - last_spi_tm_ms >= LOG_SPI_TIMING_INTERVAL_MS) {
            last_spi_tm_ms = now_ms;
            HalSpiTelemetry tm = {};
            hal_sensor_spi_get_telemetry(&tm);
            hal_log("SPI: util=%.1f%% bus_tx=%lu busy=%luus was_us=%lu/%lu imu_us=%lu/%lu act_us=%lu/%lu tx(was=%lu imu=%lu act=%lu) sw=%lu(wi=%lu iw=%lu oth=%lu) sw_gap_us(wi=%lu/%lu iw=%lu/%lu) sens_sw(wi=%lu iw=%lu) sens_gap_us(wi=%lu/%lu iw=%lu/%lu) miss(imu=%lu was=%lu)",
                    tm.bus_utilization_pct,
                    (unsigned long)tm.bus_transactions,
                    (unsigned long)tm.bus_busy_us,
                    (unsigned long)tm.was_last_us,
                    (unsigned long)tm.was_max_us,
                    (unsigned long)tm.imu_last_us,
                    (unsigned long)tm.imu_max_us,
                    (unsigned long)tm.actuator_last_us,
                    (unsigned long)tm.actuator_max_us,
                    (unsigned long)tm.was_transactions,
                    (unsigned long)tm.imu_transactions,
                    (unsigned long)tm.actuator_transactions,
                    (unsigned long)tm.client_switches,
                    (unsigned long)tm.was_to_imu_switches,
                    (unsigned long)tm.imu_to_was_switches,
                    (unsigned long)tm.other_switches,
                    (unsigned long)tm.was_to_imu_gap_last_us,
                    (unsigned long)tm.was_to_imu_gap_max_us,
                    (unsigned long)tm.imu_to_was_gap_last_us,
                    (unsigned long)tm.imu_to_was_gap_max_us,
                    (unsigned long)tm.sensor_was_to_imu_switches,
                    (unsigned long)tm.sensor_imu_to_was_switches,
                    (unsigned long)tm.sensor_was_to_imu_gap_last_us,
                    (unsigned long)tm.sensor_was_to_imu_gap_max_us,
                    (unsigned long)tm.sensor_imu_to_was_gap_last_us,
                    (unsigned long)tm.sensor_imu_to_was_gap_max_us,
                    (unsigned long)tm.imu_deadline_miss,
                    (unsigned long)tm.was_deadline_miss);
        }
#endif

        // Maintain fixed timing with minimal jitter.
        vTaskDelayUntil(&next_wake, interval);
    }
}

// ===================================================================
// task_slow – background services on Core 0 (ADR-007)
//
// Absorbs former commTask + loop() + maintTask responsibilities:
//   - HW status monitoring (~1 Hz)
//   - DBG.loop() for TCP console
//   - Serial/TCP CLI polling
//   - Setup Wizard (CONFIG mode only)
//   - Periodic telemetry (5s)
//   - Watchdog feed
//   - SD card flush (every 2 s via sdLoggerTick())
//   - NTRIP state machine tick (every 1 s via ntripTick())
//   - ETH link monitoring (every 1 s via sdLoggerEthMonitor())
//   - GPIO mode-toggle polling (CONFIG <-> WORK)
//
// May block on I/O (TCP, SD). Core 0 is exclusively for slow path.
// ===================================================================
static void taskSlowFunc(void* param) {
    (void)param;
    hal_log("task_slow: started on core %d", xPortGetCoreID());

    // Wait for network + modules to stabilise
    vTaskDelay(pdMS_TO_TICKS(2000));

    // --- Local state (moved from former commTaskFunc + loop()) ---
    uint32_t slow_dbg_count = 0;
    uint32_t slow_freq_start = hal_millis();
    uint32_t last_hw_status_ms = 0;
    static const uint32_t HW_STATUS_INTERVAL_MS = 1000;
    uint32_t last_hw_err_log_ms = 0;
    uint8_t last_hw_err_count = 0xFF;

    // GPIO mode-toggle polling (Phase 3)
    static bool s_last_safety_for_mode = false;
    static uint32_t s_last_safety_change_ms = 0;
    static constexpr uint32_t MODE_TOGGLE_DEBOUNCE_MS = 500;
    static bool s_mode_toggle_initialized = false;

    // SD/NTRIP/ETH interval timers (task_slow integration)
    static const uint32_t SD_FLUSH_INTERVAL_MS = 2000;
    static const uint32_t NTRIP_TICK_INTERVAL_MS = 1000;
    uint32_t last_sd_flush_ms = 0;
    uint32_t last_ntrip_tick_ms = 0;

    // CLI input buffer (moved from loopCli)
    static char s_cli_buf[128];
    static size_t s_cli_len = 0;

    const TickType_t poll_interval = pdMS_TO_TICKS(10);  // 100 Hz polling
    TickType_t next_wake = xTaskGetTickCount();

    for (;;) {
        // --- Debug Console: TCP accept/input/disconnect (non-blocking) ---
        DBG.loop();

        // --- Setup Wizard (CONFIG mode only) ---
        if (modeGet() == OpMode::CONFIG && setupWizardConsumePending()) {
            setupWizardRun();
        }

        // --- Watchdog feed ---
        esp_task_wdt_reset();

        // --- Hardware status monitoring (~1 Hz) ---
        uint32_t now = hal_millis();
        if (now - last_hw_status_ms >= HW_STATUS_INTERVAL_MS) {
            last_hw_status_ms = now;

            bool safety_ok = true;
            bool steer_quality_ok = false;
            uint32_t steer_ts_ms = 0;
            bool imu_quality_ok = false;
            uint32_t imu_ts_ms = 0;
            {
                StateLock lock;
                safety_ok = g_nav.safety.safety_ok;
                steer_quality_ok = g_nav.steer.steer_angle_quality_ok;
                steer_ts_ms = g_nav.steer.steer_angle_timestamp_ms;
                imu_quality_ok = g_nav.imu.imu_quality_ok;
                imu_ts_ms = g_nav.imu.imu_timestamp_ms;
            }

            const bool steer_angle_valid =
                dep_policy::isSteerAngleInputValid(now, steer_ts_ms, steer_quality_ok);

            const bool imu_hw_detected = moduleSysIsActive(ModuleId::IMU) &&
                moduleSysGet(ModuleId::IMU)->state.detected;
            const bool imu_data_valid =
                imu_hw_detected && dep_policy::isImuInputValid(now, imu_ts_ms, imu_quality_ok);
            (void)imu_data_valid;

            const uint8_t err_count = hwStatusUpdate(
                hal_net_is_connected(),
                safety_ok,
                steer_angle_valid,
                imu_hw_detected,
                moduleSysIsActive(ModuleId::NTRIP),
                moduleSysIsActive(ModuleId::IMU),
                moduleSysIsActive(ModuleId::WAS),
                moduleSysIsActive(ModuleId::SAFETY)
            );

            bool changed = (err_count != last_hw_err_count);
            bool reminder = (err_count > 0) &&
                            shouldLogPeriodic(now, &last_hw_err_log_ms, MAIN_HW_ERR_REMINDER_MS);
            if (changed || reminder) {
                last_hw_err_count = err_count;
                last_hw_err_log_ms = now;
            }
            if (changed) {
                hal_log("SLOW: HW error count -> %u", (unsigned)err_count);
            } else if (reminder) {
                hal_log("SLOW: %u HW error(s) active", (unsigned)err_count);
            }
        }

        // --- GPIO mode-toggle polling (Phase 3) ---
        // Monitors the safety pin for CONFIG <-> WORK transitions.
        // Safety LOW -> CONFIG (emergency stop / configuration).
        // Safety HIGH -> WORK (if steer pipeline is ready).
        {
            const bool safety_now = hal_safety_ok();
            
            // Initialize on first iteration
            if (!s_mode_toggle_initialized) {
                s_last_safety_for_mode = safety_now;
                s_mode_toggle_initialized = true;
            }
            
            if (safety_now != s_last_safety_for_mode &&
                (now - s_last_safety_change_ms >= MODE_TOGGLE_DEBOUNCE_MS)) {
                s_last_safety_for_mode = safety_now;
                s_last_safety_change_ms = now;
                
                if (safety_now && modeGet() == OpMode::CONFIG) {
                    // Safety HIGH + current CONFIG -> try WORK
                    if (modeSet(OpMode::WORK)) {
                        hal_log("SLOW: GPIO mode-toggle -> WORK (safety HIGH)");
                    } else {
                        hal_log("SLOW: GPIO mode-toggle -> WORK rejected (pipeline incomplete)");
                    }
                } else if (!safety_now && modeGet() == OpMode::WORK) {
                    // Safety LOW + current WORK -> CONFIG
                    modeSet(OpMode::CONFIG);
                    hal_log("SLOW: GPIO mode-toggle -> CONFIG (safety LOW)");
                    // Disconnect NTRIP TCP if connected
#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
                    if (hal_tcp_connected()) {
                        hal_log("SLOW: NTRIP disconnect (mode -> CONFIG)");
                        hal_tcp_disconnect();
                    }
#endif
                }
            }
        }

        // --- Periodic telemetry (5s, suppressed during CLI activity) ---
        {
            static uint32_t s_last_telemetry_ms = 0;
            static constexpr uint32_t TELEMETRY_INTERVAL_MS = 5000;
            static constexpr uint32_t CLI_QUIET_MS = 2000;
            if (now - s_last_telemetry_ms >= TELEMETRY_INTERVAL_MS) {
                if (now - s_cli_last_rx_ms >= CLI_QUIET_MS) {
                    s_last_telemetry_ms = now;

                    float heading_deg = 0.0f;
                    float steer_angle_deg = 0.0f;
                    int steer_angle_raw = 0;
                    bool safety_ok = false;
                    bool work_switch = false;
                    bool steer_switch = false;
                    float gps_speed_kmh = 0.0f;
                    float desired_steer_angle = 0.0f;
                    bool watchdog_triggered = false;
                    int pid_output = 0;
                    bool settings_received = false;
                    float roll_deg = 0.0f;
                    float yaw_rate_dps = 0.0f;
                    bool imu_quality_ok = false;
                    uint32_t imu_timestamp_ms = 0;

                    {
                        StateLock lock;
                        heading_deg = g_nav.imu.heading_deg;
                        steer_angle_deg = g_nav.steer.steer_angle_deg;
                        steer_angle_raw = (int)g_nav.steer.steer_angle_raw;
                        safety_ok = g_nav.safety.safety_ok;
                        work_switch = g_nav.sw.work_switch;
                        steer_switch = g_nav.sw.steer_switch;
                        gps_speed_kmh = g_nav.sw.gps_speed_kmh;
                        desired_steer_angle = g_nav.sw.desiredSteerAngleDeg;
                        watchdog_triggered = g_nav.safety.watchdog_triggered;
                        pid_output = (int)g_nav.pid.pid_output;
                        settings_received = g_nav.pid.settings_received;
                        roll_deg = g_nav.imu.roll_deg;
                        yaw_rate_dps = g_nav.imu.yaw_rate_dps;
                        imu_quality_ok = g_nav.imu.imu_quality_ok;
                        imu_timestamp_ms = g_nav.imu.imu_timestamp_ms;
                    }

                    const uint32_t imu_age_ms =
                        (imu_timestamp_ms == 0U) ? 0U : (uint32_t)(now - imu_timestamp_ms);
                    hal_log("STAT: hd=%.1f st=%.1f raw=%d safety=%s work=%s steer=%s spd=%.1f wdog=%s pid=%d tgt=%.1f roll=%.2f yaw=%.2f imu_ok=%s imu_age=%lums net=%s cfg=%s mode=%s",
                            heading_deg,
                            steer_angle_deg,
                            steer_angle_raw,
                            safety_ok ? "OK" : "KICK",
                            work_switch ? "ON" : "OFF",
                            steer_switch ? "ON" : "OFF",
                            gps_speed_kmh,
                            watchdog_triggered ? "TRIG" : "OK",
                            pid_output,
                            desired_steer_angle,
                            roll_deg,
                            yaw_rate_dps,
                            imu_quality_ok ? "Y" : "N",
                            (unsigned long)imu_age_ms,
                            hal_net_is_connected() ? "UP" : "DOWN",
                            settings_received ? "Y" : "N",
                            modeToString(modeGet()));
                } else {
                    s_last_telemetry_ms = now;  // suppress during CLI
                }
            }
        }

        // --- Serial CLI input (moved from loopCli) ---
        while (Serial.available()) {
            const int ch = Serial.read();
            s_cli_last_rx_ms = hal_millis();
            if (ch == '\r' || ch == '\n') {
                if (s_cli_len > 0) {
                    s_cli_buf[s_cli_len] = '\0';
                    DBG.println();
                    cliProcessLine(s_cli_buf);
                    s_cli_len = 0;
                }
            } else if (ch == 3) {  // Ctrl+C
                s_cli_len = 0;
                DBG.println("^C");
            } else if (ch == 8 || ch == 127) {  // Backspace / DEL
                if (s_cli_len > 0) {
                    s_cli_len--;
                    DBG.print("\b \b");
                }
            } else if (s_cli_len + 1 < sizeof(s_cli_buf)) {
                s_cli_buf[s_cli_len++] = static_cast<char>(ch);
                DBG.print(static_cast<char>(ch));
            }
        }

        // --- ETH link monitoring (every ~1 s) ---
        sdLoggerEthMonitor();

        // --- NTRIP state machine tick (every 1 s, WORK mode + ETH only) ---
#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
        if (now - last_ntrip_tick_ms >= NTRIP_TICK_INTERVAL_MS) {
            last_ntrip_tick_ms = now;
            if (modeGet() == OpMode::WORK) {
                if (!hal_net_is_connected()) {
                    if (hal_tcp_connected()) {
                        hal_log("SLOW: NTRIP disconnect (ETH link down)");
                        hal_tcp_disconnect();
                    }
                } else {
                    ntripTick();
                }
            } else {
                // CONFIG mode — do not attempt NTRIP connections.
                if (hal_tcp_connected()) {
                    hal_log("SLOW: NTRIP disconnect (not in WORK mode)");
                    hal_tcp_disconnect();
                }
            }
        }
#endif

        // --- SD card flush (every 2 s) ---
        if (now - last_sd_flush_ms >= SD_FLUSH_INTERVAL_MS) {
            last_sd_flush_ms = now;
            sdLoggerTick();
        }

        // --- UM980 console tick ---
        um980SetupConsoleTick();

        // --- Debug heartbeat ---
        if (MAIN_VERBOSE_TASK_DBG) {
            slow_dbg_count++;
            if (slow_dbg_count % 500 == 0) {
                uint32_t freq_now = hal_millis();
                float hz = (slow_dbg_count * 1000.0f) / (float)(freq_now - slow_freq_start);
                slow_freq_start = freq_now;
                slow_dbg_count = 0;
                DBG.printf("[DBG-SLOW] %.1f Hz\r\n", hz);
            }
        }

        vTaskDelayUntil(&next_wake, poll_interval);
    }
}

// ===================================================================
// Arduino setup() — Orchestrates boot phases
// ===================================================================
void setup() {
    const uint32_t t_boot_start = hal_millis();

    // Phase 1: Hardware & HAL
    bootInitHal();

    // Phase 2: Module System (ADR-MODULE-002)
    bootInitModules();

    // Phase 2.5: Debug Console (TCP/Telnet server — as early as possible)
    bootInitDebugConsole();

    // Phase 3: Configuration
    bootInitConfig();

    // Phase 4: Communication
    bootInitCommunication();

    // Phase 5: Operating Mode Decision
    bootEnterMode();

    // Phase 6: Control System
    bootInitControl();

    // Phase 7: Start FreeRTOS Tasks
    bootStartTasks();

    hal_log("BOOT: total ... %lu ms", (unsigned long)(hal_millis() - t_boot_start));
}

// ===================================================================
// Arduino loop() — Empty (ADR-007)
//
// All runtime logic moved to task_fast (Core 1) and task_slow (Core 0).
// loop() only feeds the WDT as safety net.
// ===================================================================
void loop() {
    // Safety net: WDT feed in case task_slow hasn't started yet
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1000));
}
