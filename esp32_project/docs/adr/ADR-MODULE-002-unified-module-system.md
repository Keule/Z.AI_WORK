# ADR-MODULE-002: Unified Module System

- Status: accepted
- Datum: 2025-06-27
- Supersedes: ADR-MODULE-001

## Kontext

ADR-MODULE-001 definierte einen Pflichtvertrag fuer hardware-nahe Module
(IsEnabled/Init/Update/IsHealthy + ModState), aber:
- Nur 2 von 6 Pflichtmodulen erfuellen den Vertrag vollstaendig
- Kein Modul hat einen vollstaendigen ModState (error_code fehlt ueberall)
- CLI-Befehle sind modulspezifisch implementiert
- Resource-Management bei Deaktivierung fehlt (Pins/Timer/Tasks/UART)
- Drei Modi (BOOTING/ACTIVE/PAUSED) sind ueberkomplex
- Funktionale Teile (WiFi, BT, OTA, GNSS, Safety) sind keine Module
- Nicht-modulare Teile (LOGSW, SD) sind als Module definiert

## Entscheidung

### Modi: CONFIG und WORK

| Modus | Bedeutung | Control Loop | Netzwerk | CLI |
|-------|-----------|--------------|----------|-----|
| CONFIG | Konfigurierbar, alles ruhig | Gestoppt | Aktiv | Voll |
| WORK | Autosteer aktiv | 200 Hz Pipeline | Aktiv | Einschraenkt |

- Boot ist transient, geht automatisch nach init in CONFIG.
- Kein PAUSED mehr. CONFIG ist das neue PAUSED.

### Modulliste (13 Module)

```cpp
enum class ModuleId : uint8_t {
    ETH = 0,        // Transport: W5500 Ethernet
    WIFI,           // Transport: WiFi (AP/STA)
    BT,             // Transport: Bluetooth SPP
    NETWORK,        // Protokoll: PGN Codec RX/TX
    GNSS,           // Sensor: UM980 UART (2x)
    NTRIP,          // Service: RTCM Client
    IMU,            // Sensor: BNO085 SPI
    WAS,            // Sensor: ADS1118 / Wheel Angle Sensor
    ACTUATOR,       // Aktor: DRV8263 SPI
    SAFETY,         // Sensor: Safety-Loop GPIO + Watchdog
    STEER,          // Logik: PID Controller
    LOGGING,        // Service: SD-Card Logger
    OTA,            // Service: OTA Update
    COUNT
};
```

Gestrichene "Module": logsw (Konfig-Parameter von LOGGING), sd (Geraeteknoten).

### Abhaengigkeitsgraph

```
NETWORK: mindestens ETH oder WIFI aktiv
NTRIP:   NETWORK + GNSS
STEER:   IMU + WAS + ACTUATOR + SAFETY
OTA:     mindestens ETH oder WIFI oder BT aktiv
```

### Einheitliche Modulschnittstelle

Jedes Modul MUSS folgende Funktionen implementieren (No-Op wenn kein Beitrag):

```cpp
// Lifecycle
bool   mod_<name>_is_enabled(void);
void   mod_<name>_activate(void);
void   mod_<name>_deactivate(void);
bool   mod_<name>_is_healthy(uint32_t now_ms);

// Pipeline (nur in WORK)
ModuleResult mod_<name>_input(uint32_t now_ms);
ModuleResult mod_<name>_process(uint32_t now_ms);
ModuleResult mod_<name>_output(uint32_t now_ms);

// Konfiguration
bool mod_<name>_cfg_get(const char* key, char* buf, size_t len);
bool mod_<name>_cfg_set(const char* key, const char* val);
bool mod_<name>_cfg_apply(void);
bool mod_<name>_cfg_save(void);
bool mod_<name>_cfg_load(void);
bool mod_<name>_cfg_show(void);
bool mod_<name>_debug(void);
```

### ModuleResult

```cpp
struct ModuleResult { bool success; uint32_t error_code; };
```

### ModState (aus ADR-MODULE-001, beibehalten)

```cpp
struct ModState {
    bool     detected;       // Hardware erkannt
    bool     quality_ok;     // Qualitaet in Grenzwerten
    uint32_t last_update_ms; // Letzte Aktualisierung
    int32_t  error_code;     // 0 = OK
};
```

### is_healthy() Prueft alle 4 Bedingungen

```cpp
bool is_healthy = state.detected
               && state.quality_ok
               && (now_ms - state.last_update_ms) <= freshness_timeout_ms
               && state.error_code == 0;
```

### ModuleOps2 Registry

```cpp
struct ModuleOps2 {
    const char*    name;
    ModuleId       id;
    bool           (*is_enabled)(void);
    void           (*activate)(void);
    void           (*deactivate)(void);
    bool           (*is_healthy)(uint32_t);
    ModuleResult   (*input)(uint32_t);
    ModuleResult   (*process)(uint32_t);
    ModuleResult   (*output)(uint32_t);
    bool           (*cfg_get)(const char*, char*, size_t);
    bool           (*cfg_set)(const char*, const char*);
    bool           (*cfg_apply)(void);
    bool           (*cfg_save)(void);
    bool           (*cfg_load)(void);
    bool           (*cfg_show)(void);
    bool           (*debug)(void);
    const ModuleId* deps;
};
```

### Resource-Management bei Deaktivierung

deactivate() MUSS:
- GPIO-Pins freigeben (hal_pin_claim_release)
- Timer/Interrupts stoppen
- Dynamischen Speicher freigeben
- FreeRTOS-Tasks loeschen
- UART flush + close

### CLI (einheitlich fuer alle Module)

```
module <name> show              Zustand + Health + Konfig
module <name> set <key> <val>   Konfigwert setzen (RAM)
module <name> load              aus NVS laden
module <name> apply             Konfig aktivieren
module <name> save              nach NVS schreiben
module <name> activate          aktivieren (Dep-Check)
module <name> deactivate        deaktivieren
module <name> debug             Lifecycle-Check
module list                     alle Module + Status
mode config | work              Modus wechseln
```

### No-Op Konvention

Jedes Modul hat ALLE Funktionen implementiert.
Kein Beitrag = expliziter No-Op der `{true, 0}` zurueckgibt.
 Niemals nullptr in der Registry.

### Invarianten

1. In CONFIG: NUR activate/deactivate/cfg-*/debug. Keine Pipeline.
2. In WORK: input/process/output zyklisch. activate/deactivate mit Warnung.
3. is_healthy() hat keine Seiteneffekte.
4. error_code ist je Modul dokumentiert und stabil.
5. Bei activate() mit fehlender Abhaengigkeit: Fehlermeldung mit Grund.

### DoD-Kriterien (Phase 4)

1. API-Vollstaendigkeit: Alle 13 Module haben 15+1 Funktionen
2. State-Vollstaendigkeit: Alle Module haben ModState mit 4 Feldern
3. Health-Konsistenz: is_healthy() prueft alle 4 Bedingungen
4. Freshness: is_healthy() wird false bei Timeout
5. Resource-Management: deactivate() gibt alle Ressourcen frei
6. CLI: Generisch, keine modulspezifischen Handler
7. NVS: cfg_save/cfg_load funktioniert fuer alle Module

## Konsequenzen

### Positiv

- Einheitliche Schnittstelle fuer alle 13 Module
- Klare Trennung CONFIG (konfigurieren) / WORK (autosteer)
- Vollstaendiges Resource-Management
- Generische CLI ohne modulspezifischen Code
- NVS-Persistenz fuer alle Module einheitlich

### Negativ

- Initialer Refactoring-Aufwand (13 Module neu schreiben)
- main.cpp und Task-Struktur muessen angepasst werden
- Bestehende config_*.cpp / cmd_*.cpp werden obsolet
