/**
 * @file DebugConsole.cpp
 * @brief Network Console — Fan-Out Multiplexer for USB-Serial + TCP/Telnet.
 *
 * Uses raw lwIP BSD sockets (lwip_socket/lwip_bind/lwip_listen/lwip_accept)
 * instead of WiFiServer/WiFiClient.  Same underlying API that WiFiServer uses
 * internally, but with:
 *   - Deferred bind with retry (network may not be ready at boot)
 *   - Explicit error logging on bind failure
 *   - No silent failures
 *
 * All lwIP socket calls use the lwip_ prefix (lwip_socket, lwip_bind, etc.)
 * matching the pattern in ESP32's WiFiServer.cpp.
 */

#include "DebugConsole.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>
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
        return;
    }
    closeClient();
    closeServerSocket();
    _tcp_port = tcp_port;
    _tcp_enabled = true;
    _bound = false;
    _server_fd = -1;
    _client_fd = -1;
    _last_bind_attempt = 0;
    _targets |= DBG_TARGET_TCP;
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
        lwip_close(_server_fd);
        _server_fd = -1;
    }
    _bound = false;
}

void DebugConsole::closeClient() {
    if (_client_fd >= 0) {
        lwip_close(_client_fd);
        _client_fd = -1;
    }
    _iac_pos = 0;
    _last_tcp_byte = 0;
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

    char dummy;
    ssize_t n = lwip_recv(_client_fd, &dummy, 1, MSG_DONTWAIT | MSG_PEEK);
    if (n == 0) {
        closeClient();
        return false;
    }
    if (n < 0) {
        int err = errno;
        if (err == EWOULDBLOCK || err == EAGAIN) {
            return true;
        }
        closeClient();
        return false;
    }
    return true;
}

// ===================================================================
// Deferred bind — called from loop()
// ===================================================================
bool DebugConsole::tryBind() {
    if (_bound) return true;

    uint32_t now = millis();
    if (now - _last_bind_attempt < BIND_RETRY_MS) {
        return false;
    }
    _last_bind_attempt = now;

    if (_server_fd >= 0) {
        lwip_close(_server_fd);
        _server_fd = -1;
    }

    _server_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0) {
        Serial.printf("[DBG] socket() failed: errno=%d\n", errno);
        return false;
    }

    int opt = 1;
    lwip_setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Non-blocking
    int flags = lwip_fcntl(_server_fd, F_GETFL, 0);
    if (flags >= 0) lwip_fcntl(_server_fd, F_SETFL, flags | O_NONBLOCK);

    // Disable Nagle
    int nodelay = 1;
    lwip_setsockopt(_server_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(_tcp_port);

    if (lwip_bind(_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        Serial.printf("[DBG] bind(port %u) failed: errno=%d (retrying...)\n",
                       (unsigned)_tcp_port, errno);
        lwip_close(_server_fd);
        _server_fd = -1;
        return false;
    }

    if (lwip_listen(_server_fd, 1) < 0) {
        Serial.printf("[DBG] listen() failed: errno=%d\n", errno);
        lwip_close(_server_fd);
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
//
// TCP target: lone \n is automatically converted to \r\n so that
// terminal emulators (Putty, etc.) receive proper CR+LF line endings.
// Serial (USB-CDC) keeps receiving \n only — the host-side driver
// usually handles CR translation.
//
// We track _last_tcp_byte to avoid emitting \r\r\n when the caller
// already sent \r\n (e.g. "foo\r\n" should stay "foo\r\n", not
// "foo\r\r\n").
//

size_t DebugConsole::write(uint8_t b) {
    if (_targets & DBG_TARGET_SERIAL) {
        Serial.write(b);
    }

    if ((_targets & DBG_TARGET_TCP) && _client_fd >= 0) {
        // Insert CR before lone LF
        if (b == '\n' && _last_tcp_byte != '\r') {
            uint8_t cr = '\r';
            lwip_send(_client_fd, &cr, 1, MSG_DONTWAIT);
            _tcp_bytes_written++;
        }
        ssize_t n = lwip_send(_client_fd, &b, 1, MSG_DONTWAIT);
        if (n > 0) {
            _tcp_bytes_written++;
            _last_tcp_byte = b;
        } else {
            _tcp_drop_count++;
        }
    }

    return 1;
}

size_t DebugConsole::write(const uint8_t* buffer, size_t size) {
    if (size == 0) return 0;

    // Serial: send the whole buffer in one shot (efficient)
    if (_targets & DBG_TARGET_SERIAL) {
        Serial.write(buffer, size);
    }

    // TCP: inline CR-before-LF conversion, do NOT delegate to the
    // single-byte write() — it would also write to Serial again.
    if ((_targets & DBG_TARGET_TCP) && _client_fd >= 0) {
        // Local staging buffer.  Worst case: every byte needs a CR prefix.
        uint8_t tmp[TCP_WRITE_CHUNK * 2];
        size_t tmp_len = 0;

        for (size_t i = 0; i < size; i++) {
            // Insert CR before lone LF
            if (buffer[i] == '\n' && _last_tcp_byte != '\r') {
                tmp[tmp_len++] = '\r';
            }
            tmp[tmp_len++] = buffer[i];
            _last_tcp_byte = buffer[i];

            // Flush when staging buffer is full
            if (tmp_len >= TCP_WRITE_CHUNK) {
                ssize_t n = lwip_send(_client_fd, tmp, tmp_len, MSG_DONTWAIT);
                if (n > 0) {
                    _tcp_bytes_written += n;
                } else {
                    _tcp_drop_count += tmp_len;
                }
                tmp_len = 0;
            }
        }

        // Flush remainder
        if (tmp_len > 0) {
            ssize_t n = lwip_send(_client_fd, tmp, tmp_len, MSG_DONTWAIT);
            if (n > 0) {
                _tcp_bytes_written += n;
            } else {
                _tcp_drop_count += tmp_len;
            }
        }
    }

    return size;
}

void DebugConsole::flush() {
    Serial.flush();
}

// ===================================================================
// Stream interface (input — not used directly)
// ===================================================================
int DebugConsole::available() { return 0; }
int DebugConsole::peek() { return -1; }
int DebugConsole::read() { return -1; }

// ===================================================================
// Loop — deferred bind / accept / read / disconnect
// ===================================================================
void DebugConsole::loop() {
    if (!_tcp_enabled) return;

    if (!_bound) {
        tryBind();
        if (!_bound) return;
    }

    acceptNewClient();
    readClientInput();
}

void DebugConsole::acceptNewClient() {
    if (_server_fd < 0 || !_bound) return;

    if (_client_fd >= 0) {
        if (!isTcpClientConnected()) {
            // Disconnected — cleaned up by isTcpClientConnected()
        } else {
            // Still connected — check for new client (reject: 1 client max)
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_fd = lwip_accept(_server_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (new_fd >= 0) {
                const char* msg = "\r\nConnection taken over by another client.\r\n";
                lwip_send(_client_fd, msg, strlen(msg), MSG_DONTWAIT);
                lwip_close(_client_fd);
                _client_fd = new_fd;
                _tcp_connect_count++;
                int nodelay = 1;
                lwip_setsockopt(_client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
                const char* welcome =
                    "\r\n=== AgSteer Remote Console ===\r\n"
                    "Device: AgSteer ESP32-S3\r\n"
                    "Type 'help' for available commands.\r\n\r\n";
                lwip_send(_client_fd, welcome, strlen(welcome), MSG_DONTWAIT);
                Serial.println("=== AgSteer Remote Console === (new client)");
            }
            return;
        }
    }

    // No client — try accept
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_fd = lwip_accept(_server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (new_fd < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            Serial.printf("[DBG] accept() error: errno=%d\n", errno);
        }
        return;
    }

    _client_fd = new_fd;
    _tcp_connect_count++;

    int flags = lwip_fcntl(_client_fd, F_GETFL, 0);
    if (flags >= 0) lwip_fcntl(_client_fd, F_SETFL, flags | O_NONBLOCK);
    int nodelay = 1;
    lwip_setsockopt(_client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    // Welcome banner (via DebugConsole → Serial + TCP)
    Serial.println("=== AgSteer Remote Console ===");
    Serial.println("Device: AgSteer ESP32-S3");
    Serial.println("Type 'help' for available commands.");
    Serial.println();
}

// ===================================================================
// Input handling
// ===================================================================
void DebugConsole::readClientInput() {
    if (_client_fd < 0 || !_input_cb) return;

    char buf[32];
    while (true) {
        ssize_t n = lwip_recv(_client_fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (n <= 0) {
            if (n == 0) {
                closeClient();
            } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
                Serial.printf("[DBG] recv() error: errno=%d\n", errno);
                closeClient();
            }
            break;
        }

        for (ssize_t i = 0; i < n; i++) {
            const uint8_t c = static_cast<uint8_t>(buf[i]);
            _tcp_bytes_read++;

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
                continue;
            }

            _input_cb(c);
        }
    }
}

// ===================================================================
// Telnet negotiation
// ===================================================================
void DebugConsole::handleTelnetNegotiation(uint8_t cmd, uint8_t opt) {
    uint8_t response[3] = {IAC, 0, opt};

    switch (cmd) {
        case DO:
            if (opt == OPT_ECHO || opt == OPT_SGA) {
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
            return;
        default:
            return;
    }

    if (_client_fd >= 0) {
        lwip_send(_client_fd, response, 3, MSG_DONTWAIT);
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
