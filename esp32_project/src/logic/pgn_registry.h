/**
 * @file pgn_registry.h
 * @brief PGN registry — centralized PGN directory and dispatch infrastructure.
 *
 * Provides:
 *   - PgnDirection enum (IN = from AgIO, OUT = to AgIO, BIDIR = both)
 *   - PgnEntry struct (PGN number, name, direction, expected payload size)
 *   - Complete PGN table (all known AgOpenGPS PGNs)
 *   - pgnLookup() for finding PGN info
 *   - pgnIsKnown() for quick check if a PGN is registered
 *
 * This header has NO Arduino/ESP32 dependencies.
 */

#pragma once

#include "pgn_types.h"
#include <cstddef>
#include <cstdint>

// ===================================================================
// PGN Direction
// ===================================================================
enum class PgnDirection : uint8_t {
    IN,     // AgIO -> Module (we receive)
    OUT,    // Module -> AgIO (we send)
    BIDIR   // Both directions
};

// ===================================================================
// PGN Registry Entry
// ===================================================================
struct PgnEntry {
    uint8_t      pgn;          // PGN number
    const char*  name;         // Human-readable name
    PgnDirection direction;    // Traffic direction
    uint8_t      payload_size; // Expected payload size (0 = variable)
};

// ===================================================================
// Complete PGN Table
// ===================================================================
constexpr PgnEntry kPgnTable[] = {
    // Network Discovery
    { 0xC8, "HelloFromAgIO",    PgnDirection::IN,    3  },
    { 0xC9, "SubnetChange",     PgnDirection::IN,    5  },
    { 0xCA, "ScanRequest",      PgnDirection::IN,    3  },
    { 0xCB, "SubnetReply",      PgnDirection::OUT,   7  },

    // Hello Replies (PGN = Src for replies)
    { 0x7E, "HelloReplySteer",  PgnDirection::OUT,   5  },
    { 0x78, "HelloReplyGps",    PgnDirection::OUT,   5  },

    // Steering
    { 0xFE, "SteerDataIn",      PgnDirection::IN,    8  },
    { 0xFD, "SteerStatusOut",   PgnDirection::OUT,   8  },
    { 0xFC, "SteerSettingsIn",  PgnDirection::IN,    8  },
    { 0xFB, "SteerConfigIn",    PgnDirection::IN,    8  },
    { 0xFA, "FromAutosteer2",   PgnDirection::OUT,   8  },

    // GPS
    { 0xD6, "GpsMainOut",       PgnDirection::OUT,   51 },

    // Machine
    { 0xEF, "MachineDataIn",    PgnDirection::IN,    8  },
    { 0xEE, "MachineConfigIn",  PgnDirection::IN,    3  },

    // Diagnostics
    { 0xDD, "HardwareMessage",  PgnDirection::BIDIR, 0  },  // variable length
};

constexpr size_t kPgnTableCount = sizeof(kPgnTable) / sizeof(kPgnTable[0]);

// ===================================================================
// Lookup Functions
// ===================================================================

/**
 * Find a PGN in the registry table.
 * @param pgn  PGN number to look up
 * @return     Pointer to PgnEntry, or nullptr if not found.
 */
inline const PgnEntry* pgnLookup(uint8_t pgn) {
    for (size_t i = 0; i < kPgnTableCount; i++) {
        if (kPgnTable[i].pgn == pgn) return &kPgnTable[i];
    }
    return nullptr;
}

/**
 * Quick check if a PGN is known/registered.
 */
inline bool pgnIsKnown(uint8_t pgn) {
    return pgnLookup(pgn) != nullptr;
}

/**
 * Get direction for a PGN. Returns BIDIR if unknown.
 */
inline PgnDirection pgnGetDirection(uint8_t pgn) {
    const PgnEntry* e = pgnLookup(pgn);
    return e ? e->direction : PgnDirection::BIDIR;
}
