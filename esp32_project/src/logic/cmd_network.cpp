/**
 * @file cmd_network.cpp
 * @brief CLI network commands — network status, RTCM telemetry.
 *
 * Uses the new module system (ModuleId::NETWORK).
 */

#include "cli.h"
#include "module_interface.h"
#include "mod_network.h"
#include "hal/hal.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

extern Stream* s_cli_out;

namespace {

void cliCmdNetwork(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->println("usage: network <show>");
        return;
    }

    if (std::strcmp(argv[1], "show") == 0) {
        s_cli_out->printf("NETWORK: active=%d\n",
                          moduleSysIsActive(ModuleId::NETWORK));

        // Transport status
        s_cli_out->printf("  ETH:   active=%d connected=%d IP=%s\n",
                          moduleSysIsActive(ModuleId::ETH),
                          hal_net_is_connected(),
                          hal_net_is_connected() ? "?" : "N/A");

        // RTCM telemetry
        NetRtcmTelemetry rtcm = {};
        mod_network_get_rtcm_telemetry(&rtcm);
        s_cli_out->printf("  RTCM: rx=%lu bytes, forwarded=%lu bytes, dropped=%lu pkts\n",
                          static_cast<unsigned long>(rtcm.rx_bytes),
                          static_cast<unsigned long>(rtcm.forwarded_bytes),
                          static_cast<unsigned long>(rtcm.dropped_packets));
        s_cli_out->printf("         overflow=%lu bytes, partial_writes=%lu\n",
                          static_cast<unsigned long>(rtcm.overflow_bytes),
                          static_cast<unsigned long>(rtcm.partial_writes));
        return;
    }

    s_cli_out->println("usage: network <show>");
}

}  // namespace

void cmd_network_register(void) {
    (void)cliRegisterCommand("network", &cliCmdNetwork, "Network status + RTCM telemetry");
}
