/**
 * @file DebugConsole.cpp
 * @brief Network Console — Fan-Out Multiplexer for USB-Serial + TCP/Telnet.
 *
 * Implementation notes:
 *   - TCP writes are non-blocking: checks availableForWrite() before
 *     writing.  If the send buffer is full, data is dropped and counted.
 *   - Telnet negotiation: responds WILL ECHO + WILL SGA to client,
 *     DONT/WONT for everything else.  This ensures raw character mode.
 *   - WiFiServer/WiFiClient are used for TCP.  On ESP32 with Ethernet
 *     (W5500 via ETH.h), WiFiServer binds to INADDR_ANY which includes
 *     all network interfaces (confirmed by WebServer working over ETH).
 *   - The global DBG object is constructed before setup() with TCP
 *     disabled.  begin() enables TCP; end() disables it again.
 */

#include "DebugConsole.h"

#include <WiFi.h>

// ===================================================================
// Global instance
// ===================================================================
DebugConsole DBG;

// ===================================================================
// Telnet protocol constants
// ===================================================================
namespace {
    constexpr uint8_t IAC  = 0xFF;  // Interpret As Command
    constexpr uint8_t WILL = 0xFB;
    constexpr uint8_t WONT = 0xFC;
    constexpr uint8_t DO   = 0xFD;
    constexpr uint8_t DONT = 0xFE;

    constexpr uint8_t OPT_ECHO = 0x01;   // Echo
    constexpr uint8_t OPT_SGA  = 0x03;   // Suppress Go Ahead
}  // namespace

// ===================================================================
// Construction / Destruction
// ===================================================================
DebugConsole::DebugConsole() = default;

DebugConsole::~DebugConsole() {
    end();
}

// ===================================================================
// Initialization
// ===================================================================
void DebugConsole::begin(uint16_t tcp_port) {
    if (_server) {
        end();  // Clean up existing server
    }

    _tcp_port = tcp_port;
    _server = new WiFiServer(_tcp_port);
    if (_server) {
        _server->begin();
        _server->setNoDelay(true);  // Disable Nagle for low latency
    }
    _tcp_enabled = true;
    _targets |= DBG_TARGET_TCP;
}

void DebugConsole::end() {
    closeClient();

    if (_server) {
        _server->stop();
        delete _server;
        _server = nullptr;
    }

    _tcp_enabled = false;
    _targets &= ~DBG_TARGET_TCP;
}

// ===================================================================
// TCP control
// ===================================================================
void DebugConsole::enableTcp(bool enable) {
    if (enable && !_tcp_enabled) {
        if (!_server) {
            _server = new WiFiServer(_tcp_port);
            if (_server) {
                _server->begin();
                _server->setNoDelay(true);
            }
        }
        _tcp_enabled = true;
        _targets |= DBG_TARGET_TCP;
    } else if (!enable && _tcp_enabled) {
        closeClient();
        if (_server) {
            _server->stop();
            delete _server;
            _server = nullptr;
        }
        _tcp_enabled = false;
        _targets &= ~DBG_TARGET_TCP;
    }
}

bool DebugConsole::isTcpClientConnected() {
    return _client && _client.connected();
}

// ===================================================================
// Print interface — Fan-Out
// ===================================================================
size_t DebugConsole::write(uint8_t b) {
    // Always write to USB-Serial
    if (_targets & DBG_TARGET_SERIAL) {
        Serial.write(b);
    }

    // Optionally write to TCP client (non-blocking)
    if ((_targets & DBG_TARGET_TCP) && _client && _client.connected()) {
        if (_client.availableForWrite() > 0) {
            _client.write(b);
            _tcp_bytes_written++;
        } else {
            _tcp_drop_count++;
        }
    }

    return 1;
}

size_t DebugConsole::write(const uint8_t* buffer, size_t size) {
    if (size == 0) return 0;

    // Always write to USB-Serial
    if (_targets & DBG_TARGET_SERIAL) {
        Serial.write(buffer, size);
    }

    // Optionally write to TCP client (non-blocking, chunked)
    if ((_targets & DBG_TARGET_TCP) && _client && _client.connected()) {
        size_t offset = 0;
        while (offset < size) {
            const size_t avail = static_cast<size_t>(_client.availableForWrite());
            if (avail == 0) {
                // Send buffer full — drop the rest
                _tcp_drop_count += (size - offset);
                break;
            }
            const size_t chunk = (size - offset < avail)
                ? (size - offset)
                : (avail < TCP_WRITE_CHUNK ? avail : TCP_WRITE_CHUNK);
            const size_t written = _client.write(buffer + offset, chunk);
            if (written == 0) {
                _tcp_drop_count += (size - offset);
                break;
            }
            _tcp_bytes_written += written;
            offset += written;
        }
    }

    return size;
}

void DebugConsole::flush() {
    Serial.flush();
    if (_client && _client.connected()) {
        _client.flush();
    }
}

// ===================================================================
// Stream interface (input — not used directly, input via callback)
// ===================================================================
int DebugConsole::available() {
    // DBG does not provide input through Stream interface.
    // Input is handled via setInputCallback() → loop().
    return 0;
}

int DebugConsole::peek() {
    return -1;  // No data available
}

int DebugConsole::read() {
    return -1;  // No data available
}

// ===================================================================
// Loop — TCP accept / read / disconnect
// ===================================================================
void DebugConsole::loop() {
    if (!_tcp_enabled || !_server) return;

    // --- Check for new connection ---
    if (!_client || !_client.connected()) {
        // Clean up stale client
        if (_client) {
            _client.stop();
        }
        // Accept new client
        WiFiClient newClient = _server->available();
        if (newClient && newClient.connected()) {
            _client = newClient;
            _tcp_connect_count++;
            _client.setNoDelay(true);

            // Welcome banner (goes through DebugConsole → Serial + TCP)
            println("=== AgSteer Remote Console ===");
            printf("Device: AgSteer ESP32-S3\n");
            printf("Type 'help' for available commands.\n");
            println();
        }
    } else {
        // Already have a client — check if a NEW client is waiting
        WiFiClient newClient = _server->available();
        if (newClient && newClient.connected()) {
            // Replace: notify old client, accept new
            _client.println();
            _client.println("Connection taken over by another client.");
            _client.flush();
            _client.stop();

            _client = newClient;
            _tcp_connect_count++;
            _client.setNoDelay(true);

            println("=== AgSteer Remote Console ===");
            printf("Device: AgSteer ESP32-S3\n");
            printf("Type 'help' for available commands.\n");
            println();
        }
    }

    // --- Clean up disconnected client ---
    if (_client && !_client.connected()) {
        _client.stop();
    }

    // --- Read input from connected client ---
    readClientInput();
}

// ===================================================================
// Input handling
// ===================================================================
void DebugConsole::readClientInput() {
    if (!_client || !_client.connected() || !_input_cb) return;

    while (_client.available()) {
        const int raw = _client.read();
        if (raw < 0) break;
        const uint8_t c = static_cast<uint8_t>(raw);
        _tcp_bytes_read++;

        // --- Telnet negotiation detection ---
        if (c == IAC && _iac_pos == 0) {
            _iac_buf[0] = c;
            _iac_pos = 1;
            continue;
        }

        if (_iac_pos > 0 && _iac_pos < 3) {
            _iac_buf[_iac_pos++] = c;
            if (_iac_pos == 3) {
                handleTelnetNegotiation(_iac_buf[1], _iac_buf[2]);
                _iac_pos = 0;
            }
            continue;  // Don't pass negotiation bytes to callback
        }

        // --- Normal character → callback ---
        _input_cb(c);
    }
}

// ===================================================================
// Telnet negotiation (minimal: echo + character mode)
// ===================================================================
void DebugConsole::handleTelnetNegotiation(uint8_t cmd, uint8_t opt) {
    uint8_t response[3] = {IAC, 0, opt};

    switch (cmd) {
        case DO:
            // Client says "please do X"
            if (opt == OPT_ECHO) {
                response[1] = WILL;  // OK, we'll echo
                _client.write(response, 3);
            } else if (opt == OPT_SGA) {
                response[1] = WILL;  // OK, character mode
                _client.write(response, 3);
            } else {
                response[1] = DONT;  // Refuse
                _client.write(response, 3);
            }
            break;

        case WILL:
            // Client says "I can do X"
            if (opt == OPT_SGA) {
                response[1] = DO;   // Please use character mode
                _client.write(response, 3);
            } else {
                response[1] = DONT;  // Don't need it
                _client.write(response, 3);
            }
            break;

        case WONT:
        case DONT:
            // Client refuses — nothing to do
            break;

        default:
            // Other IAC commands (SB, etc.) — ignore
            break;
    }
}

void DebugConsole::closeClient() {
    if (_client && _client.connected()) {
        _client.stop();
    }
    _iac_pos = 0;
}

void DebugConsole::setInputCallback(InputCallback cb) {
    _input_cb = cb;
}
