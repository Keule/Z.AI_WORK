/**
 * @file cmd_ntrip.cpp
 * @brief CLI NTRIP commands — NTRIP client status and control.
 *
 * Uses the new module system (ModuleId::NTRIP) with fallback to
 * old ntrip.h API for state queries.
 */

#include "cli.h"
#include "module_interface.h"
#include "features.h"

#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
#include "ntrip.h"
#include "global_state.h"
#endif

#include <Arduino.h>
#include <cstdio>
#include <cstring>

extern Stream* s_cli_out;

namespace {

void cliCmdNtrip(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->println("usage: ntrip <show|status>");
        return;
    }

    if (std::strcmp(argv[1], "show") == 0) {
        s_cli_out->printf("NTRIP: module active=%d\n",
                          moduleSysIsActive(ModuleId::NTRIP));
#if FEAT_ENABLED(FEAT_COMPILED_NTRIP)
        NtripState state = ntripGetState();
        const char* conn_str = "UNKNOWN";
        switch (state.conn_state) {
            case NtripConnState::IDLE:           conn_str = "IDLE"; break;
            case NtripConnState::CONNECTING:     conn_str = "CONNECTING"; break;
            case NtripConnState::AUTHENTICATING: conn_str = "AUTHENTICATING"; break;
            case NtripConnState::CONNECTED:      conn_str = "CONNECTED"; break;
            case NtripConnState::ERROR:          conn_str = "ERROR"; break;
            case NtripConnState::DISCONNECTED:   conn_str = "DISCONNECTED"; break;
        }
        s_cli_out->printf("  state=%s rx=%lu bytes fwd=%lu bytes\n",
                          conn_str,
                          static_cast<unsigned long>(state.rx_bytes),
                          static_cast<unsigned long>(state.forwarded_bytes));
        s_cli_out->printf("  connect_failures=%lu http_status=%u\n",
                          static_cast<unsigned long>(state.connect_failures),
                          static_cast<unsigned>(state.last_http_status));
        if (state.last_error[0] != '\0') {
            s_cli_out->printf("  last_error: %s\n", state.last_error);
        }
#else
        s_cli_out->println("  (NTRIP not compiled in)");
#endif
        return;
    }

    if (std::strcmp(argv[1], "status") == 0) {
        s_cli_out->printf("NTRIP active=%d\n",
                          moduleSysIsActive(ModuleId::NTRIP));
        return;
    }

    s_cli_out->println("usage: ntrip <show|status>");
}

}  // namespace

void cmd_ntrip_register(void) {
    (void)cliRegisterCommand("ntrip", &cliCmdNtrip, "NTRIP client status");
}
