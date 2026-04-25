/**
 * @file main.cpp
 * @brief ESP32-S3 firmware main entry point.
 *
 * Target: LilyGO T-ETH-Lite-S3 (ESP32-S3-WROOM-1 + W5500 Ethernet)
 *
 * Three FreeRTOS tasks:
 *   - commTask    (Core 0): HW status monitoring, GPIO poll
 *   - maintTask   (Core 0): SD flush, NTRIP connect, ETH monitor [TASK-029]
 *   - controlTask (Core 1): 200 Hz module pipeline (input → process → output)
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
 *   bootStartTasks()       — Startup errors, FreeRTOS tasks
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
// Task handles
// ===================================================================
static TaskHandle_t s_control_task_handle = nullptr;
static TaskHandle_t s_comm_task_handle = nullptr;

// Forward declarations
static void controlTaskFunc(void* param);
static void commTaskFunc(void* param);

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
        // NOTE: NTRIP tick/read/forward is handled by maintTask (sdLoggerMaintInit).
        // Do NOT call ntripTick() here — it blocks up to 5 s in hal_tcp_connect()
        // and would starve the IDLE task / trigger WDT when maintTask runs concurrently.
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
// Uses the new OpMode enum (CONFIG/WORK) from module_interface.h.
//
// NOTE: The old op_mode.h (BOOTING/ACTIVE/PAUSED) is NOT included
//       to avoid OpMode name conflict with module_interface.h.
//       GPIO mode toggle via safety pin is deferred to a future update.
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
// Boot Phase 7: Start FreeRTOS Tasks
//
// Erstellt: controlTask, commTask
// NOTE: maintTask is created by mod_logging.activate() -> sdLoggerMaintInit()
//       if SD card is detected, or by main.cpp for NTRIP-only scenarios.
// ===================================================================
static void bootStartTasks(void) {
    // -----------------------------------------------------------------
    // Maintenance Task (TASK-029)
    // -----------------------------------------------------------------
    // WICHTIG (TASK-045): Der maintTask MUSS erstellt werden wenn NTRIP
    // aktiv ist, da ntripTick() blocking TCP-Connect enthaelt (5s Timeout).
    // mod_logging.activate() already calls sdLoggerMaintInit() if SD is
    // detected.  But if only NTRIP is active (no SD), we must start it here.
    // -----------------------------------------------------------------
    const bool logging_active = moduleSysIsActive(ModuleId::LOGGING);
    const auto* log_mod = moduleSysGet(ModuleId::LOGGING);
    const bool sd_detected = logging_active && log_mod && log_mod->state.detected;
    const bool ntrip_active = moduleSysIsActive(ModuleId::NTRIP);

    if (!sd_detected && ntrip_active) {
        // NTRIP active but no SD — maintTask needed for ntripTick()
        sdLoggerMaintInit();
    } else if (sd_detected) {
        hal_log("Main: maintTask already started by LOGGING module");
    } else {
        hal_log("Main: maintenance task not started (LOGGING and NTRIP inactive)");
    }

    // Startup-Errors melden (UDP wenn Netz up, sonst Serial)
    hal_delay_ms(100);
    mod_network_send_startup_errors();

    // Control Task auf Core 1 — runs the 200 Hz module pipeline
    if ((feat::act() && feat::safety()) && s_control_pipeline_ready) {
        xTaskCreatePinnedToCore(
            controlTaskFunc,
            "ctrl",
            4096,
            nullptr,
            configMAX_PRIORITIES - 2,  // hohe Prioritaet
            &s_control_task_handle,
            1   // Core 1
        );
    } else {
        if (!(feat::act() && feat::safety())) {
            hal_log("Main: control task not started (feature disabled)");
        } else {
            hal_log("Main: control task not started (pipeline inactive)");
        }
    }

    // Communication Task auf Core 0 — HW status monitoring
    xTaskCreatePinnedToCore(
        commTaskFunc,
        "comm",
        4096,
        nullptr,
        configMAX_PRIORITIES - 3,  // etwas niedrigere Prioritaet
        &s_comm_task_handle,
        0   // Core 0
    );

    hal_log("Main: tasks created, entering main loop");
}

// ===================================================================
// Control Task – runs at 200 Hz on Core 1
//
// Executes the unified module pipeline: input → process → output
// Only runs when mode == WORK.
// ===================================================================
static void controlTaskFunc(void* param) {
    (void)param;
    hal_log("Control: task started on core %d", xPortGetCoreID());

    // Wait for network + sensors to stabilise
    vTaskDelay(pdMS_TO_TICKS(500));

    const TickType_t interval = pdMS_TO_TICKS(5);  // 200 Hz = 5 ms
    TickType_t next_wake = xTaskGetTickCount();
    uint32_t ctrl_dbg_count = 0;
    uint32_t ctrl_freq_start = hal_millis();
#if FEAT_ENABLED(FEAT_COMPILED_IMU) || FEAT_ENABLED(FEAT_COMPILED_ADS) || FEAT_ENABLED(FEAT_COMPILED_ACT)
    uint32_t last_spi_tm_ms = 0;
#endif

    for (;;) {
        // ADR-MODULE-002: run module pipeline only in WORK mode
        if (modeGet() == OpMode::WORK) {
            const uint32_t now_ms = hal_millis();

            // === Module Pipeline ===
            moduleSysRunInput(now_ms);
            moduleSysRunProcess(now_ms);
            moduleSysRunOutput(now_ms);
        }

        if (MAIN_VERBOSE_TASK_DBG) {
            // Heartbeat DBG every 1s (= every 200 iterations)
            ctrl_dbg_count++;
            if (ctrl_dbg_count % 200 == 0) {
                uint32_t freq_now = hal_millis();
                float hz = (ctrl_dbg_count * 1000.0f) / (float)(freq_now - ctrl_freq_start);
                ctrl_freq_start = freq_now;
                ctrl_dbg_count = 0;
                DBG.printf("[DBG-CTRL] %.1f Hz\r\n", hz);
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

        // Maintain fixed 200 Hz timing with minimal jitter.
        vTaskDelayUntil(&next_wake, interval);
    }
}

// ===================================================================
// Communication Task – runs on Core 0
//
// HW status monitoring only. All module I/O (net, ntrip, etc.) is
// handled by the controlTask pipeline or by the maintTask.
// ===================================================================
static void commTaskFunc(void* param) {
    (void)param;
    hal_log("Comm: task started on core %d", xPortGetCoreID());

    // Wait for network to initialise (done in setup, but give time to settle)
    vTaskDelay(pdMS_TO_TICKS(2000));

    DBG.println("[DBG-COMM] wait done, entering poll loop");

    const TickType_t poll_interval = pdMS_TO_TICKS(10);  // 100 Hz polling
    TickType_t next_wake = xTaskGetTickCount();

    // Hardware status update runs at ~1 Hz
    static uint32_t s_last_hw_status_ms = 0;
    static const uint32_t HW_STATUS_INTERVAL_MS = 1000;
    uint32_t comm_dbg_count = 0;
    uint32_t comm_freq_start = hal_millis();
    uint32_t last_hw_err_log_ms = 0;
    uint8_t last_hw_err_count = 0xFF;

    for (;;) {
        // NO pipeline here — controlTask handles all module I/O

        if (MAIN_VERBOSE_TASK_DBG) {
            // Heartbeat DBG every 5s (= every 500 iterations)
            comm_dbg_count++;
            if (comm_dbg_count % 500 == 0) {
                uint32_t freq_now = hal_millis();
                float hz = (comm_dbg_count * 1000.0f) / (float)(freq_now - comm_freq_start);
                comm_freq_start = freq_now;
                comm_dbg_count = 0;
                DBG.printf("[DBG-COMM] %.1f Hz\r\n", hz);
            }
        }

        // Hardware status monitoring (~1 Hz)
        uint32_t now = hal_millis();
        if (now - s_last_hw_status_ms >= HW_STATUS_INTERVAL_MS) {
            s_last_hw_status_ms = now;

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

            // Use new module system for IMU HW detection
            const bool imu_hw_detected = moduleSysIsActive(ModuleId::IMU) &&
                moduleSysGet(ModuleId::IMU)->state.detected;
            const bool imu_data_valid =
                imu_hw_detected && dep_policy::isImuInputValid(now, imu_ts_ms, imu_quality_ok);

            // Use new module system for module active checks
            const uint8_t err_count = hwStatusUpdate(
                hal_net_is_connected(),                     // Ethernet connected
                safety_ok,                                  // Safety circuit OK
                steer_angle_valid,                          // steer angle freshness + plausibility
                imu_hw_detected,                            // IMU hardware presence
                moduleSysIsActive(ModuleId::NTRIP),         // NTRIP module active
                moduleSysIsActive(ModuleId::IMU),           // IMU module active
                moduleSysIsActive(ModuleId::WAS),           // WAS module active
                moduleSysIsActive(ModuleId::SAFETY)         // SAFETY module active
            );

            (void)imu_data_valid;

            // Log only on count changes, plus occasional reminders.
            bool changed = (err_count != last_hw_err_count);
            bool reminder = (err_count > 0) &&
                            shouldLogPeriodic(now, &last_hw_err_log_ms, MAIN_HW_ERR_REMINDER_MS);
            if (changed || reminder) {
                last_hw_err_count = err_count;
                last_hw_err_log_ms = now;
            }

            if (changed) {
                hal_log("COMM: HW error count changed -> %u", (unsigned)err_count);
            } else if (reminder) {
                hal_log("COMM: %u HW error(s) active", (unsigned)err_count);
            }
        }

        // DBG.println("[DBG-COMM] looped");
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
// Arduino loop() — Clean dispatcher
// ===================================================================
static uint32_t s_loop_dbg_count = 0;
// s_cli_last_rx_ms declared above (needed by TCP input callback in bootInitCommunication)
static constexpr uint32_t MAIN_CLI_QUIET_LOG_MS = 2000;

/// Periodische Serial-Telemetrie (alle 5s, netzwerkunabhaengig)
static void loopTelemetry(void) {
    static uint32_t s_last_status = 0;
    uint32_t now = hal_millis();
    if (now - s_last_status < 5000) return;

    // CLI-Aktivitaet erkannt? Telemetrie zurueckhalten.
    if (now - s_cli_last_rx_ms < MAIN_CLI_QUIET_LOG_MS) {
        s_last_status = now;
        return;
    }

    s_last_status = now;

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
    hal_log("STAT: hd=%.1f st=%.1f raw=%d safety=%s work=%s steer=%s spd=%.1f wdog=%s pid=%d tgt=%.1f roll_deg=%.2f yaw_rate_dps=%.2f imu_quality_ok=%s imu_age_ms=%lu net=%s cfg=%s mode=%s",
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
}

/// Serial CLI Input-Verarbeitung
static void loopCli(void) {
    static char s_cli_buf[128];
    static size_t s_cli_len = 0;

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
}

/// Debug Heartbeat (nur wenn MAIN_VERBOSE_TASK_DBG aktiv)
static void loopDebugHeartbeat(void) {
    if (!MAIN_VERBOSE_TASK_DBG) return;

    s_loop_dbg_count++;
    if (s_loop_dbg_count <= 5 || s_loop_dbg_count % 10 == 0) {
        static uint32_t s_loop_freq_start_ms = 0;
        static uint32_t s_loop_freq_samples = 0;
        if (s_loop_freq_start_ms == 0) s_loop_freq_start_ms = hal_millis();
        s_loop_freq_samples++;
        if (s_loop_freq_samples >= 10) {
            const uint32_t freq_now_ms = hal_millis();
            const float hz =
                (s_loop_freq_samples * 1000.0f) / (float)(freq_now_ms - s_loop_freq_start_ms);
            s_loop_freq_start_ms = freq_now_ms;
            s_loop_freq_samples = 0;
            DBG.printf("[DBG-LOOP] %.1f Hz\r\n", hz);
        }
    }
}

void loop() {
    // Debug Console: TCP accept/input/disconnect (non-blocking)
    DBG.loop();

    // Setup Wizard only in CONFIG mode
    if (modeGet() == OpMode::CONFIG && setupWizardConsumePending()) {
        setupWizardRun();
    }

    // Watchdog fuettern
    esp_task_wdt_reset();

    // Periodische Telemetrie (5s Interval)
    loopTelemetry();

    // Debug Heartbeat
    loopDebugHeartbeat();

    // Serial CLI
    loopCli();

    vTaskDelay(pdMS_TO_TICKS(100));
}
