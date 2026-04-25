/**
 * @file DebugConsole.h
 * @brief Network Console — Fan-Out Multiplexer for USB-Serial + TCP/Telnet.
 *
 * DebugConsole inherits from Print, so all print/println/printf/write calls
 * are automatically fanned out to USB-Serial AND an optional TCP client.
 *
 * Architecture (Fan-Out / Fan-In):
 *
 *     DBG.print("hello")
 *         |
 *         v
 *   DebugConsole  (Print)
 *     |        \
 *     v         v
 *   Serial   TCP-Client
 *
 *   TCP-Client-Input --> callback --> cliProcessLine()
 *
 * Usage:
 *   DBG.begin(23);              // TCP port 23 (telnet)
 *   DBG.enableTcp(true);
 *   DBG.setInputCallback(onInput);
 *   cliSetOutput(&DBG);         // Redirect CLI output
 *
 *   // In loop():
 *   DBG.loop();                 // Handle TCP connections (non-blocking)
 *
 * Connect:
 *   telnet <device-ip> 23
 *   nc <device-ip> 23
 *
 * Features:
 *   - \r status-line overwrite works (ANSI \033[K passes through)
 *   - Basic telnet negotiation (WILL ECHO, WILL SGA, refuses everything else)
 *   - Non-blocking TCP writes: drops data if send buffer full (no stall)
 *   - Max 1 TCP client at a time; new client replaces old
 *   - No dynamic memory allocation in the hot path
 *   - Thread-safe: hal_log() calls are already mutex-protected by caller
 *
 * Extensibility:
 *   - Additional output targets (UDP, WebSocket) can be added as extra
 *     write targets in write() / write(const uint8_t*, size_t).
 *   - See DBG_TARGET_TCP for how a target is integrated.
 *
 * Why NOT UDP for status lines:
 *   UDP is connectionless and does not preserve ordering of overlapping
 *   writes.  A status line (\r overwrite) sent via UDP could arrive
 *   after subsequent full-line output, garbling the display.  TCP
 *   guarantees in-order delivery which is essential for \r-based
 *   status-line updates.
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Stream.h>
#include <cstdint>
#include <cstddef>

// ===================================================================
// Output target flags (bitfield, extensible for future targets)
// ===================================================================
#define DBG_TARGET_SERIAL  0x01   ///< USB CDC Serial (always available)
#define DBG_TARGET_TCP     0x02   ///< TCP/Telnet client (when connected)
// Future: DBG_TARGET_UDP  0x04
// Future: DBG_TARGET_WS   0x08

class DebugConsole : public Stream {
public:
    DebugConsole();
    ~DebugConsole();

    // -----------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------
    /// Start the debug console. TCP server begins listening immediately.
    /// Serial output is always active (even before begin()).
    /// @param tcp_port  TCP port to listen on (default 23 = telnet).
    void begin(uint16_t tcp_port = 23);

    /// Stop TCP server and disconnect client. Serial output remains active.
    void end();

    // -----------------------------------------------------------------
    // TCP control
    // -----------------------------------------------------------------
    /// Enable or disable TCP server (can be toggled at runtime).
    void enableTcp(bool enable);
    bool isTcpEnabled() const { return _tcp_enabled; }

    /// Check if a TCP client is currently connected.
    bool isTcpClientConnected();

    /// Get the TCP port being listened on.
    uint16_t getTcpPort() const { return _tcp_port; }

    // -----------------------------------------------------------------
    // Loop (call regularly from main loop / Arduino loop())
    // -----------------------------------------------------------------
    /// Accept new connections, read client input, detect disconnects.
    /// Non-blocking: returns immediately if nothing to do.
    /// MUST be called regularly for TCP to function.
    void loop();

    // -----------------------------------------------------------------
    // Input callback
    // -----------------------------------------------------------------
    /// Callback type for receiving characters from TCP client.
    using InputCallback = void (*)(uint8_t c);

    /// Set a callback for TCP client input characters.
    /// Characters are delivered one at a time.
    /// If no callback is set, TCP input is silently discarded.
    void setInputCallback(InputCallback cb);

    // -----------------------------------------------------------------
    // Print interface (fan-out to all active targets)
    // -----------------------------------------------------------------
    size_t write(uint8_t b) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    void flush() override;

    // Stream interface (input — not used directly, input via callback)
    int available() override;
    int peek() override;
    int read() override;

    // -----------------------------------------------------------------
    // Target control (runtime)
    // -----------------------------------------------------------------
    /// Enable or disable specific output targets (bitfield).
    /// Example: DBG.setTargets(DBG_TARGET_SERIAL);  // TCP only
    void setTargets(uint8_t targets) { _targets = targets; }
    uint8_t getTargets() const { return _targets; }

    // -----------------------------------------------------------------
    // Telemetry / Debug info
    // -----------------------------------------------------------------
    uint32_t getTcpBytesWritten() const { return _tcp_bytes_written; }
    uint32_t getTcpBytesRead() const { return _tcp_bytes_read; }
    uint32_t getTcpConnectCount() const { return _tcp_connect_count; }
    uint32_t getTcpDropCount() const { return _tcp_drop_count; }

private:
    void acceptNewClient();
    void readClientInput();
    void closeClient();
    void handleTelnetNegotiation(uint8_t cmd, uint8_t opt);

    // Configuration
    bool        _tcp_enabled = false;
    uint16_t    _tcp_port = 23;
    uint8_t     _targets = DBG_TARGET_SERIAL;  // Active output targets

    // TCP server + client
    WiFiServer* _server = nullptr;   // Allocated in begin(), freed in end()
    WiFiClient  _client;             // Stack-allocated, invalid until connected

    // Input
    InputCallback _input_cb = nullptr;

    // Telnet negotiation state
    uint8_t     _iac_buf[3] = {};
    uint8_t     _iac_pos = 0;

    // Telemetry
    uint32_t    _tcp_bytes_written = 0;
    uint32_t    _tcp_bytes_read = 0;
    uint32_t    _tcp_connect_count = 0;
    uint32_t    _tcp_drop_count = 0;

    // TCP write tuning
    static constexpr size_t TCP_WRITE_CHUNK = 256;
};

// ===================================================================
// Global instance (defined in DebugConsole.cpp)
// ===================================================================
extern DebugConsole DBG;
