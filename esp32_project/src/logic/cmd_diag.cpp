/**
 * @file cmd_diag.cpp
 * @brief CLI diag command — selftest, modules, pins, log, export, hw, mem, net.
 *
 * Split from cli.cpp (Step 7).
 */

#include "cli.h"
#include "diag.h"
#include "diagnostics.h"

#include <cstdio>
#include <cstring>

extern Stream* s_cli_out;

namespace {

void cliCmdDiag(int argc, char** argv) {
    if (argc < 2) {
        s_cli_out->println("usage: diag <selftest|modules|pins|log|export|hw|mem|net>");
        return;
    }

    if (std::strcmp(argv[1], "selftest") == 0) {
        DiagSelftestResult result = diagRunSelftest();
        s_cli_out->println("=== Selftest ===");
        for (int i = 0; i < result.count; ++i) {
            const auto& m = result.modules[i];
            const char* status = m.passed ? "OK  " : "FAIL";
            s_cli_out->printf("  %-10s [%s]%s%s\n",
                              m.name ? m.name : "?",
                              status,
                              m.detail ? " " : "",
                              m.detail ? m.detail : "");
        }
        s_cli_out->printf("  --- %d/%d bestanden, %d fehlgeschlagen ---\n",
                          result.passed, result.count, result.failed);
        return;
    }
    if (std::strcmp(argv[1], "modules") == 0) {
        diagPrintModuleStatus(s_cli_out);
        return;
    }
    if (std::strcmp(argv[1], "pins") == 0) {
        diagPrintPinMap(s_cli_out);
        return;
    }
    if (std::strcmp(argv[1], "log") == 0) {
        diagPrintLogStats();
        return;
    }
    if (std::strcmp(argv[1], "export") == 0) {
        diagExportLogCsv();
        return;
    }
    if (std::strcmp(argv[1], "hw") == 0) {
        diagPrintHw();
        return;
    }
    if (std::strcmp(argv[1], "mem") == 0) {
        diagPrintMem();
        return;
    }
    if (std::strcmp(argv[1], "net") == 0) {
        diagPrintNet();
        return;
    }

    s_cli_out->println("usage: diag <selftest|modules|pins|log|export|hw|mem|net>");
}

}  // namespace

void cmd_diag_register(void) {
    (void)cliRegisterCommand("diag", &cliCmdDiag, "Diagnostics (selftest/modules/pins/log/export/hw/mem/net)");
}
