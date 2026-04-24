/**
 * @file sd_ota_version.cpp
 * @brief Platform-independent version parsing and comparison for SD-OTA.
 *
 * Implements sdOtaParseVersion(), sdOtaCompareVersion() and
 * sdOtaGetCurrentVersion() from sd_ota.h.
 *
 * No Arduino / ESP32 / platform-specific headers used.
 */

#include "logic/sd_ota.h"
#include <cstdlib>
#include <cstring>

// ===================================================================
// Version parsing
// ===================================================================
bool sdOtaParseVersion(const char* str, SdOtaVersion* out) {
    if (!out) return false;
    if (!str) { out->major = out->minor = out->patch = 0; return false; }

    // Zero-init output
    out->major = out->minor = out->patch = 0;

    // Accept "1.2.3" or "1.2.3\n" or " 1.2.3 "
    // strtol will skip leading whitespace
    char* end = nullptr;
    unsigned long major = std::strtoul(str, &end, 10);
    if (end == str || *end != '.') return false;

    str = end + 1;  // skip '.'
    unsigned long minor = std::strtoul(str, &end, 10);
    if (end == str) return false;

    // patch is optional – if no '.', patch = 0
    unsigned long patch = 0;
    if (*end == '.') {
        str = end + 1;
        patch = std::strtoul(str, &end, 10);
    }

    // Reject if any component exceeds uint16_t range
    if (major > 65535 || minor > 65535 || patch > 65535) return false;

    out->major = static_cast<uint16_t>(major);
    out->minor = static_cast<uint16_t>(minor);
    out->patch = static_cast<uint16_t>(patch);
    return true;
}

// ===================================================================
// Version comparison
// ===================================================================
int sdOtaCompareVersion(const SdOtaVersion* a, const SdOtaVersion* b) {
    if (!a || !b) return 0;

    // Compare major, then minor, then patch
    if (a->major != b->major) return (a->major > b->major) ? 1 : -1;
    if (a->minor != b->minor) return (a->minor > b->minor) ? 1 : -1;
    if (a->patch != b->patch) return (a->patch > b->patch) ? 1 : -1;
    return 0;  // equal
}

// ===================================================================
// Current firmware version (compile-time)
// ===================================================================
SdOtaVersion sdOtaGetCurrentVersion(void) {
    SdOtaVersion v = {0, 0, 0};

#ifdef FIRMWARE_VERSION
    sdOtaParseVersion(FIRMWARE_VERSION, &v);
#endif

    return v;
}
