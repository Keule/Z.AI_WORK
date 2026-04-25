/**
 * @file DebugConsole.cpp
 * @brief Network Console — Fan-Out Multiplexer for USB-Serial + TCP/Telnet.
 *
 * Uses raw lwIP BSD sockets instead of WiFiServer/WiFiClient.
 * This avoids silent bind failures that WiFiServer suffers from when
 * the network interface (ETH.h / W5500) is not yet ready at boot time.
 *
 * Key design:
 *   - Socket creation + bind is deferred to loop() with retry logic.
 *   - If bind() fails (no network yet), it retries every BIND_RETRY_MS.
 *   - Once bound, accept() is called non-blocking in loop().
 *   - Client I/O uses send()/recv() directly — no WiFiClient wrapper.
 *   - All socket operations are non-blocking (O_NONBLOCK).
 */

#include "DebugConsole.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

// ===================================================================
// Global instance
// ===================================================================
DebugConsole DBG;

// ===================================================================
// Telnet protocol constants
// ===================================================================
namespace {
    constexpr uint8_t IAC  = 0xFF;
    constexpr uint8_t WILL = 0xFB;
    constexpr uint8_t WONT = 0xFC;
    constexpr uint8_t DO   = 0xFD;
    constexpr uint8_t DONT = 0xFE;

    constexpr uint8_t OPT_ECHO = 0x01;
    constexpr uint8_t OPT_SGA  = 0x03;
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
    if (_tcp_enabled && _tcp_port == tcp_port) {
        return;  // Already configured
    }

    // Clean up any existing state
    closeClient();
    closeServerSocket();

    _tcp_port = tcp_port;
    _tcp_enabled = true;
    _bound = false;
    _server_fd = -1;
    _client_fd = -1;
    _last_bind_attempt = 0;
    _targets |= DBG_TARGET_TCP;

    // Note: actual socket bind is deferred to loop()
    // because the network interface may not be ready yet.
}

void DebugConsole::end() {
    closeClient();
    closeServerSocket();
    _tcp_enabled = false;
    _bound = false;
    _targets &= ~DBG_TARGET_TCP;
}

void DebugConsole::closeServerSocket() {
    if (_server_fd >= 0) {
        ::close(_server_fd);
        _server_fd = -1;
    }
    _bound = false;
}

void DebugConsole::closeClient() {
    if (_client_fd >= 0) {
        ::close(_client_fd);
        _client_fd = -1;
    }
    _iac_pos = 0;
}

// ===================================================================
// TCP control
// ===================================================================
void DebugConsole::enableTcp(bool enable) {
    if (enable && !_tcp_enabled) {
        _tcp_enabled = true;
        _bound = false;
        _server_fd = -1;
        _last_bind_attempt = 0;
        _targets |= DBG_TARGET_TCP;
    } else if (!enable && _tcp_enabled) {
        end();
    }
}

bool DebugConsole::isTcpClientConnected() {
    if (_client_fd < 0) return false;

    // Check if socket is still connected using recv() with MSG_DONTWAIT + PEEK
    char dummy;
    ssize_t n = ::recv(_client_fd, &dummy, 1, MSG_DONTWAIT | MSG_PEEK);
    if (n == 0) {
        // Remote closed connection
        closeClient();
        return false;
    }
    if (n < 0) {
        int err = errno;
        if (err == EWOULDBLOCK || err == EAGAIN) {
            return true;  // Still connected, no data available
        }
        // Error — connection lost
        closeClient();
        return false;
    }
    return true;  // Data available, still connected
}

// ===================================================================
// Deferred bind — called from loop()
// ===================================================================
bool DebugConsole::tryBind() {
    if (_bound) return true;

    uint32_t now = millis();
    if (now - _last_bind_attempt < BIND_RETRY_MS) {
        return false;  // Throttle retries
    }
    _last_bind_attempt = now;

    // Close any stale socket
    if (_server_fd >= 0) {
        ::close(_server_fd);
        _server_fd = -1;
    }

    // Create socket
    _server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0) {
        Serial.printf("[DBG] socket() failed: errno=%d\n", errno);
        return false;
    }

    // Allow address reuse
    int opt = 1;
    ::setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Non-blocking
    ::fcntl(_server_fd, F_SETFL, O_NONBLOCK);

    // Disable Nagle for low latency
    int nodelay = 1;
    ::setsockopt(_server_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    // Bind to INADDR_ANY (all interfaces — ETH + WiFi)
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(_tcp_port);

    if (::bind(_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        Serial.printf("[DBG] bind(port %u) failed: errno=%d (retrying...)\n",
                       (unsigned)_tcp_port, errno);
        ::close(_server_fd);
        _server_fd = -1;
        return false;
    }

    // Listen (backlog = 1 — we only accept one client at a time)
    if (::listen(_server_fd, 1) < 0) {
        Serial.printf("[DBG] listen() failed: errno=%d\n", errno);
        ::close(_server_fd);
        _server_fd = -1;
        return false;
    }

    _bound = true;
    Serial.printf("[DBG] TCP server listening on port %u\n", (unsigned)_tcp_port);
    return true;
}

// ===================================================================
// Print interface — Fan-Out
// ===================================================================
size_t DebugConsole::write(uint8_t b) {
    // Always write to USB-Serial
    if (_targets & DBG_TARGET_SERIAL) {
        Serial.write(b);
    }

    // Write to TCP client
    if ((_targets & DBG_TARGET_TCP) && _client_fd >= 0) {
        ssize_t n = ::send(_client_fd, &b, 1, MSG_DONTWAIT);
        if (n > 0) {
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

    // Write to TCP client (chunked to limit per-call blocking)
    if ((_targets & DBG_TARGET_TCP) && _client_fd >= 0) {
        size_t offset = 0;
        while (offset < size) {
            const size_t remaining = size - offset;
            const size_t chunk = (remaining < TCP_WRITE_CHUNK) ? remaining : TCP_WRITE_CHUNK;
            ssize_t n = ::send(_client_fd, buffer + offset, chunk, MSG_DONTWAIT);
            if (n <= 0) {
                // Socket full or error — drop the rest
                _tcp_drop_count += remaining;
                break;
            }
            _tcp_bytes_written += n;
            offset += n;
        }
    }

    return size;
}

void DebugConsole::flush() {
    Serial.flush();
    // No-op for TCP — lwIP handles TCP flush internally
}

// ===================================================================
// Stream interface (input — not used directly)
// ===================================================================
int DebugConsole::available() {
    return 0;
}

int DebugConsole::peek() {
    return -1;
}

int DebugConsole::read() {
    return -1;
}

// ===================================================================
// Loop — deferred bind / accept / read / disconnect
// ===================================================================
void DebugConsole::loop() {
    if (!_tcp_enabled) return;

    // --- Deferred bind: retry if not yet bound ---
    if (!_bound) {
        tryBind();
        if (!_bound) return;  // Network not ready yet
    }

    // --- Accept new connection ---
    acceptNewClient();

    // --- Read input from connected client ---
    readClientInput();
}

void DebugConsole::acceptNewClient() {
    if (_server_fd < 0 || !_bound) return;

    if (_client_fd >= 0) {
        // Check if existing client is still connected
        if (!isTcpClientConnected()) {
            // Client disconnected — already cleaned up by isTcpClientConnected()
        } else {
            // Client still connected — check if a NEW client is waiting
            // (we only allow one client; reject additional connections)
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_fd = ::accept(_server_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (new_fd >= 0) {
                // Notify old client
                const char* msg = "\r\nConnection taken over by another client.\r\n";
                ::send(_client_fd, msg, std::strlen(msg), MSG_DONTWAIT);
                // Close old
                ::close(_client_fd);
                _client_fd = new_fd;
                _tcp_connect_count++;
                // Set options on new client
                int nodelay = 1;
                ::setsockopt(_client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
                // Welcome
                const char* welcome = "\r\n=== AgSteer Remote Console ===\r\n"
                                       "Device: AgSteer ESP32-S3\r\n"
                                       "Type 'help' for available commands.\r\n\r\n";
                ::send(_client_fd, welcome, std::strlen(welcome), MSG_DONTWAIT);
                // Also print to Serial
                Serial.println("=== AgSteer Remote Console === (new client connected)");
            }
            return;
        }
    }

    // No client connected — try to accept one
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_fd = ::accept(_server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (new_fd < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            // Unexpected error — try to recover
            Serial.printf("[DBG] accept() error: errno=%d\n", errno);
        }
        return;
    }

    // New client connected!
    _client_fd = new_fd;
    _tcp_connect_count++;

    // Set non-blocking + no-delay
    ::fcntl(_client_fd, F_SETFL, O_NONBLOCK);
    int nodelay = 1;
    ::setsockopt(_client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    // Welcome banner (via DebugConsole → Serial + TCP)
    Serial.println("=== AgSteer Remote Console ===");
    Serial.printf("Device: AgSteer ESP32-S3\n");
    Serial.printf("Type 'help' for available commands.\n");
    Serial.println();
}

// ===================================================================
// Input handling
// ===================================================================
void DebugConsole::readClientInput() {
    if (_client_fd < 0 || !_input_cb) return;

    char buf[32];
    while (true) {
        ssize_t n = ::recv(_client_fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (n <= 0) {
            if (n == 0) {
                // Remote closed connection
                closeClient();
            } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
                // Error
                Serial.printf("[DBG] recv() error: errno=%d\n", errno);
                closeClient();
            }
            break;
        }

        for (ssize_t i = 0; i < n; i++) {
            const uint8_t c = static_cast<uint8_t>(buf[i]);
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
}

// ===================================================================
// Telnet negotiation (minimal: echo + character mode)
// ===================================================================
void DebugConsole::handleTelnetNegotiation(uint8_t cmd, uint8_t opt) {
    uint8_t response[3] = {IAC, 0, opt};

    switch (cmd) {
        case DO:
            if (opt == OPT_ECHO) {
                response[1] = WILL;
            } else if (opt == OPT_SGA) {
                response[1] = WILL;
            } else {
                response[1] = WONT;
            }
            break;

        case WILL:
            if (opt == OPT_SGA) {
                response[1] = DO;
            } else {
                response[1] = DONT;
            }
            break;

        case WONT:
        case DONT:
            return;  // Nothing to respond

        default:
            return;  // Ignore SB and other commands
    }

    if (_client_fd >= 0) {
        ::send(_client_fd, response, 3, MSG_DONTWAIT);
    }
}

void DebugConsole::setInputCallback(InputCallback cb) {
    _input_cb = cb;
}

// ===================================================================
// Diagnostics
// ===================================================================
void DebugConsole::printStats(Print& out) const {
    out.printf("DBG TCP: enabled=%d port=%u targets=0x%02x\n",
               _tcp_enabled ? 1 : 0, _tcp_port, _targets);
    out.printf("  server_fd=%d bound=%d\n", _server_fd, _bound ? 1 : 0);
    out.printf("  client_fd=%d %s\n",
               _client_fd,
               (_client_fd >= 0) ? "CONNECTED" : "none");
    out.printf("  connects=%lu  tx=%lu  rx=%lu  drops=%lu\n",
               (unsigned long)_tcp_connect_count,
               (unsigned long)_tcp_bytes_written,
               (unsigned long)_tcp_bytes_read,
               (unsigned long)_tcp_drop_count);
}
