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
 * Implementation: Uses raw lwIP BSD sockets (socket/bind/listen/accept)
 * instead of WiFiServer/WiFiClient.  This is necessary because:
 *   - WiFiServer/WiFiClient are WiFi-oriented and may not bind correctly
 *     when ETH.h (W5500) is the active network interface.
 *   - Raw sockets give us explicit error handling — bind() failure is
 *     logged and retried on each loop() call until the network is ready.
 *   - No silent failures: we always know the socket state.
 *
 * Usage:
 *   DBG.begin(23);              // Request TCP port 23 (telnet)
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
 */

#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <cstdint>
#include <cstddef>
#include <lwip/sockets.h>

// ===================================================================
// Output target flags (bitfield, extensible for future targets)
// ===================================================================
#define DBG_TARGET_SERIAL  0x01   ///< USB CDC Serial (always available)
#define DBG_TARGET_TCP     0x02   ///< TCP/Telnet client (when connected)

class DebugConsole : public Stream {
public:
    DebugConsole();
    ~DebugConsole();

    // -----------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------
    /// Request TCP server on given port.  Socket bind is deferred to
    /// loop() — if the network interface is not ready yet, it retries
    /// on every loop() call until bind succeeds.
    /// Serial output is always active (even before begin()).
    void begin(uint16_t tcp_port = 23);

    /// Stop TCP server, close client, release socket.
    void end();

    // -----------------------------------------------------------------
    // TCP control
    // -----------------------------------------------------------------
    void enableTcp(bool enable);
    bool isTcpEnabled() const { return _tcp_enabled; }
    bool isTcpClientConnected();
    uint16_t getTcpPort() const { return _tcp_port; }

    // -----------------------------------------------------------------
    // Loop (call regularly from main loop / Arduino loop())
    // -----------------------------------------------------------------
    /// Deferred bind (if not yet bound), accept new connections,
    /// read client input, detect disconnects.
    /// Non-blocking: returns immediately.
    void loop();

    // -----------------------------------------------------------------
    // Input callback
    // -----------------------------------------------------------------
    using InputCallback = void (*)(uint8_t c);
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
    void setTargets(uint8_t targets) { _targets = targets; }
    uint8_t getTargets() const { return _targets; }

    // -----------------------------------------------------------------
    // Telemetry / Debug info
    // -----------------------------------------------------------------
    uint32_t getTcpBytesWritten() const { return _tcp_bytes_written; }
    uint32_t getTcpBytesRead() const { return _tcp_bytes_read; }
    uint32_t getTcpConnectCount() const { return _tcp_connect_count; }
    uint32_t getTcpDropCount() const { return _tcp_drop_count; }

    void printStats(Print& out) const;

private:
    /// Try to create socket + bind + listen.  Returns true on success.
    /// Logs failure reason via Serial.
    bool tryBind();

    void acceptNewClient();
    void readClientInput();
    void closeClient();
    void closeServerSocket();
    void handleTelnetNegotiation(uint8_t cmd, uint8_t opt);

    // Configuration
    bool        _tcp_enabled = false;
    uint16_t    _tcp_port = 23;
    uint8_t     _targets = DBG_TARGET_SERIAL;

    // TCP server socket (raw lwIP)
    int         _server_fd = -1;       ///< Listening socket (-1 = not bound)
    bool        _bound = false;        ///< true after successful bind+listen

    // TCP client socket (raw lwIP)
    int         _client_fd = -1;       ///< Connected client (-1 = none)

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

    // Bind retry throttle
    uint32_t    _last_bind_attempt = 0;
    static constexpr uint32_t BIND_RETRY_MS = 2000;  ///< Retry bind every 2s

    // TCP write tuning
    static constexpr size_t TCP_WRITE_CHUNK = 256;
};

// ===================================================================
// Global instance (defined in DebugConsole.cpp)
// ===================================================================
extern DebugConsole DBG;
