/**
 * @file cmd_ntrip.cpp
 * @brief CLI ntrip command — show, status, set, connect, disconnect.
 *
 * Split from cli.cpp (Step 7).
 */

#include "cli.h"
#include "logic/features.h"
#include "runtime_config.h"
#include "ntrip.h"

#include <cstdio>
#include <cstring>

extern Stream* s_cli_out;

namespace {

#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
const char* ntripConnStateToStr(NtripConnState state) {
    switch (state) {
        case NtripConnState::IDLE: return "IDLE";
        case NtripConnState::CONNECTING: return "CONNECTING";
        case NtripConnState::AUTHENTICATING: return "AUTHENTICATING";
        case NtripConnState::CONNECTED: return "CONNECTED";
        case NtripConnState::DISCONNECTED: return "DISCONNECTED";
        case NtripConnState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
#endif

void cliCmdNtrip(int argc, char** argv) {
#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
    RuntimeConfig& cfg = softConfigGet();
    if (argc < 2) {
        s_cli_out->println("usage: ntrip <show|status|set|connect|disconnect>");
        return;
    }

    if (std::strcmp(argv[1], "show") == 0 || std::strcmp(argv[1], "status") == 0) {
        const NtripState state = ntripGetState();
        s_cli_out->println("NTRIP:");
        s_cli_out->printf("  Host:       %s\n", cfg.ntrip_host);
        s_cli_out->printf("  Port:       %u\n", static_cast<unsigned>(cfg.ntrip_port));
        s_cli_out->printf("  Mountpoint: %s\n", cfg.ntrip_mountpoint);
        s_cli_out->printf("  User:       %s\n", cfg.ntrip_user);
        s_cli_out->printf("  Password:   %s\n", cfg.ntrip_password[0] ? "********" : "(empty)");
        s_cli_out->printf("  State:      %s\n", ntripConnStateToStr(state.conn_state));
        s_cli_out->printf("  Bytes RX:   %lu\n", static_cast<unsigned long>(state.rx_bytes));
        s_cli_out->printf("  Fwd bytes:  %lu\n", static_cast<unsigned long>(state.forwarded_bytes));
        return;
    }

    if (std::strcmp(argv[1], "set") == 0) {
        if (argc < 4) {
            s_cli_out->println("usage: ntrip set <host|port|mount|user|pass> <value>");
            return;
        }
        if (std::strcmp(argv[2], "host") == 0) {
            std::strncpy(cfg.ntrip_host, argv[3], sizeof(cfg.ntrip_host) - 1);
            cfg.ntrip_host[sizeof(cfg.ntrip_host) - 1] = '\0';
        } else if (std::strcmp(argv[2], "port") == 0) {
            cfg.ntrip_port = static_cast<uint16_t>(std::atoi(argv[3]));
        } else if (std::strcmp(argv[2], "mount") == 0 || std::strcmp(argv[2], "mountpoint") == 0) {
            std::strncpy(cfg.ntrip_mountpoint, argv[3], sizeof(cfg.ntrip_mountpoint) - 1);
            cfg.ntrip_mountpoint[sizeof(cfg.ntrip_mountpoint) - 1] = '\0';
        } else if (std::strcmp(argv[2], "user") == 0) {
            std::strncpy(cfg.ntrip_user, argv[3], sizeof(cfg.ntrip_user) - 1);
            cfg.ntrip_user[sizeof(cfg.ntrip_user) - 1] = '\0';
        } else if (std::strcmp(argv[2], "pass") == 0 || std::strcmp(argv[2], "password") == 0) {
            std::strncpy(cfg.ntrip_password, argv[3], sizeof(cfg.ntrip_password) - 1);
            cfg.ntrip_password[sizeof(cfg.ntrip_password) - 1] = '\0';
        } else {
            s_cli_out->println("usage: ntrip set <host|port|mount|user|pass> <value>");
            return;
        }

        ntripSetConfig(cfg.ntrip_host, cfg.ntrip_port, cfg.ntrip_mountpoint, cfg.ntrip_user, cfg.ntrip_password);
        s_cli_out->println("NTRIP config updated (runtime).");
        return;
    }

    if (std::strcmp(argv[1], "connect") == 0) {
        ntripSetConfig(cfg.ntrip_host, cfg.ntrip_port, cfg.ntrip_mountpoint, cfg.ntrip_user, cfg.ntrip_password);
        s_cli_out->println("NTRIP connect requested (state machine will connect).");
        return;
    }

    if (std::strcmp(argv[1], "disconnect") == 0) {
        cfg.ntrip_host[0] = '\0';
        cfg.ntrip_mountpoint[0] = '\0';
        ntripSetConfig(cfg.ntrip_host, cfg.ntrip_port, cfg.ntrip_mountpoint, cfg.ntrip_user, cfg.ntrip_password);
        s_cli_out->println("NTRIP disconnected (runtime config cleared host/mount).");
        return;
    }

    s_cli_out->println("usage: ntrip <show|status|set|connect|disconnect>");
#else
    (void)argc;
    (void)argv;
    s_cli_out->println("NTRIP not compiled in this profile.");
#endif
}

}  // namespace

void cmd_ntrip_register(void) {
    (void)cliRegisterCommand("ntrip", &cliCmdNtrip, "NTRIP runtime config and status");
}
