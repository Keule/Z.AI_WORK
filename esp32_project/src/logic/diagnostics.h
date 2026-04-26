#pragma once
#include <cstdint>
#include <Stream.h>

/// Selftest-Ergebnis fuer ein Modul
typedef struct {
    const char* name;     ///< Modul-Name (z.B. "ETH", "IMU")
    bool tested;          ///< true wenn Test durchgefuehrt
    bool passed;          ///< true wenn Test bestanden
    const char* detail;   ///< Zusaetzliche Info bei Fehler
} DiagModuleResult;

/// Alle Selftest-Ergebnisse
#define DIAG_MAX_MODULES 10
typedef struct {
    DiagModuleResult modules[DIAG_MAX_MODULES];
    int count;           ///< Anzahl getesteter Module
    int passed;          ///< Anzahl bestandener Tests
    int failed;          ///< Anzahl fehlgeschlagener Tests
} DiagSelftestResult;

/// Umfassender Selftest aller Module.
/// Gibt Ergebnisse als DiagSelftestResult zurueck.
DiagSelftestResult diagRunSelftest(void);

/// Modul-Status als Tabelle ausgeben (compiled/detected/active/pins/deps).
/// output: Stream* fuer Serial-Ausgabe.
void diagPrintModuleStatus(void* output);

/// Pin-Claim-Map ausgeben (Pin -> Owner).
/// output: Stream* fuer Serial-Ausgabe.
void diagPrintPinMap(void* output);

/// SD-Log Statistiken ausgeben (Record Count, Buffer Utilization, File Size).
void diagPrintLogStats(void);

/// SD-Log CSV ueber Serial streamen.
/// Gibt false zurueck wenn SD nicht verfuegbar oder keine Daten.
/// Nur im CONFIG Modus zulaessig!
bool diagExportLogCsv(void);
