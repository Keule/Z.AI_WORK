/**
 * @file config_framework.h
 * @brief Basis-Konfig-Framework — ADR-006 (TASK-037).
 *
 * Zentrale Registry fuer alle Konfigurations-Kategorien mit einheitlicher
 * Schnittstelle fuer Validierung, Anwenden, Speichern, Laden und Anzeige.
 *
 * Jede Kategorie registriert sich mit ConfigCategoryOps und kann
 * unabhaengig validiert und persistiert werden.
 * Aenderungen sind nur im CONFIG-Modus zulaessig.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Konfigurations-Kategorien (ADR-006).
typedef enum {
    CONFIG_CAT_NETWORK  = 0,  ///< Netzwerk (DHCP/Static, IP, GW, Subnet)
    CONFIG_CAT_NTRIP    = 1,  ///< NTRIP Client (Host, Port, Mount, Credentials)
    CONFIG_CAT_GNSS     = 2,  ///< GNSS (Baudrate, UART-Rollen)
    CONFIG_CAT_PID      = 3,  ///< PID-Regler (Kp, Ki, Kd)
    CONFIG_CAT_ACTUATOR = 4,  ///< Aktuator (Typ: SPI, Cytron, IBT2)
    CONFIG_CAT_SYSTEM   = 5,  ///< System (Log-Intervall, diverse Einstellungen)
    CONFIG_CAT_COUNT    = 6   ///< Anzahl Kategorien
} ConfigCategory;

/// Maximale Anzahl registrierbarer Kategorien.
#define CONFIG_MAX_CATEGORIES 8

/// Konfigurations-Validierungsergebnis.
typedef struct {
    bool        valid;          ///< true wenn alle Werte gueltig
    const char* error_field;    ///< nullptr wenn gueltig, sonst Feldname
    const char* error_msg;      ///< Fehlerbeschreibung (nur bei valid==false)
} ConfigValidationResult;

/// Vorwaertsdeklaration fuer Stream-Typ (Arduino Stream*).
struct ConfigCategoryOps;

/// Opaque Stream-Zeiger (Arduino Stream*).
/// Vermeidet Arduino-Header-Abhaengigkeit in diesem Header.
typedef void* ConfigStream;

/// Funktionstyp: Kategorie validieren.
typedef bool (*ConfigValidateFn)(void);

/// Funktionstyp: Kategorie auf Hardware anwenden.
typedef bool (*ConfigApplyFn)(void);

/// Funktionstyp: Kategorie aus NVS laden.
typedef bool (*ConfigLoadFn)(void);

/// Funktionstyp: Kategorie in NVS speichern.
typedef bool (*ConfigSaveFn)(void);

/// Funktionstyp: Kategorie-Werte auf Stream anzeigen.
typedef void (*ConfigShowFn)(ConfigStream output);

/// Funktionstyp: Einzelnen Wert setzen (key=value).
/// @param key    Schluessel (z.B. "kp", "ip")
/// @param value  Wert als String
/// @return true bei Erfolg, false bei unbekanntem Key oder ungueltigem Wert
typedef bool (*ConfigSetFn)(const char* key, const char* value);

/// Funktionstyp: Einzelnen Wert als String abfragen.
/// @param key    Schluessel
/// @param buf    Ausgabe-Buffer
/// @param buf_size Groesse des Buffers
/// @return true bei Erfolg, false bei unbekanntem Key
typedef bool (*ConfigGetFn)(const char* key, char* buf, size_t buf_size);

/// Kategorie-Deskriptor mit allen Operationen.
typedef struct ConfigCategoryOps {
    ConfigCategory    category;     ///< Kategorie-ID
    const char*       name;         ///< Anzeigename (deutsch)
    const char*       name_en;      ///< Anzeigename (englisch, fuer Keys)
    ConfigValidateFn  validate;     ///< Validierungsfunktion
    ConfigApplyFn     apply;        ///< Anwenden auf Hardware
    ConfigLoadFn      load;         ///< Aus NVS laden
    ConfigSaveFn      save;         ///< In NVS speichern
    ConfigShowFn      show;         ///< Werte anzeigen
    ConfigSetFn       set;          ///< Einzelnen Wert setzen
    ConfigGetFn       get;          ///< Einzelnen Wert abfragen
} ConfigCategoryOps;

// ===================================================================
// Framework API
// ===================================================================

/// Framework initialisieren (einmalig beim Boot).
/// Erstellt die Registry und registriert alle 6 Kategorien.
void configFrameworkInit(void);

/// Einzelne Kategorie in der Registry registrieren.
/// Muss nach configFrameworkInit() aufgerufen werden.
/// @return true bei Erfolg, false bei Duplikat oder voller Registry
bool configFrameworkRegisterCategory(const ConfigCategoryOps* ops);

/// Alle Kategorien validieren.
/// @return true wenn alle Kategorien gueltig
bool configFrameworkValidateAll(void);

/// Alle Kategorien anwenden (auf Hardware/Echtzeit-System).
/// @return true wenn alle Apply-Funktionen erfolgreich
bool configFrameworkApplyAll(void);

/// Alle Kategorien in NVS speichern.
/// Validiert zuerst — speichert nur wenn alle Kategorien gueltig.
/// @return true bei Erfolg
bool configFrameworkSaveAll(void);

/// Alle Kategorien aus NVS laden.
/// @return true bei Erfolg
bool configFrameworkLoadAll(void);

/// Alle Kategorien auf Werkseinstellungen zuruecksetzen.
/// Loescht gesamten NVS Namespace und laedt Defaults.
void configFrameworkFactoryReset(void);

/// Kategorie nach ID suchen.
/// @return Pointer auf Operations-Deskriptor oder nullptr
const ConfigCategoryOps* configFrameworkFindCategory(ConfigCategory cat);

/// Kategorie nach Name suchen (case-insensitiv).
/// @return Pointer auf Operations-Deskriptor oder nullptr
const ConfigCategoryOps* configFrameworkFindCategoryByName(const char* name);

/// Anzahl registrierter Kategorien.
size_t configFrameworkCategoryCount(void);

/// Einzelne Kategorie anzeigen.
void configFrameworkShowCategory(ConfigCategory cat, ConfigStream output);

/// Alle Kategorien anzeigen.
void configFrameworkShowAll(ConfigStream output);

/// Einzelnen Wert setzen ( Kategorie + Key + Value).
/// @return true bei Erfolg
bool configFrameworkSet(ConfigCategory cat, const char* key, const char* value);

/// Einzelnen Wert abfragen ( Kategorie + Key ).
/// @return true bei Erfolg
bool configFrameworkGet(ConfigCategory cat, const char* key, char* buf, size_t buf_size);

/// Pruefen ob Konfigurationsaenderungen zulaessig sind.
/// Nur im CONFIG-Modus ist Bearbeitung erlaubt.
bool configFrameworkIsEditable(void);

/// Status-Uebersicht auf Stream ausgeben.
void configFrameworkPrintStatus(ConfigStream output);

#ifdef __cplusplus
}
#endif
