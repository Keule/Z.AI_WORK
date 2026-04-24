# TASK-028 Soft-Config mit Nutzer-Defaults und Runtime-Konfiguration

- **ID**: TASK-028
- **Titel**: `soft_config.h` mit Nutzer-Defaults, dreiwertigem ModState und `RuntimeConfig` einfuehren
- **Status**: done
- **Priorität**: medium
- **Komponenten**: include/soft_config.h, src/logic/runtime_config.h, src/logic/runtime_config.cpp, src/logic/modules.cpp, src/main.cpp
- **Dependencies**: TASK-027
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
- **Owner**: ki-planer
- **Epic**: EPIC-003

- **classification**: dependent
- **exclusive_before**: [TASK-027]
- **parallelizable_after**: []

- **Origin**:
  Nutzer-Anforderung: "Was haeltst du von fw_config und soft_config? Da die eine die Firmware konfiguriert (zur compile time) und die andere die ganzen benutzereditierbaren Variablen zur runtime mit ihren Defaults enthaelt?" Erweitert: "Die aktiven Module sollten ALLE Module erfassen mit Status 1=enabled, 0=disabled, -1=nicht in fw."

- **Diskussion**:
  - Direkt: https://chat.z.ai/c/d6f6eb9b-9217-401b-bb23-08e8c0fbca69
  - Shared: https://chat.z.ai/s/a858dd17-02e3-416c-a123-649830256a4e

- **Kontext/Problem**:
  Aktuell gibt es keine zentrale Stelle fuer nutzereditierbare Konfiguration. NTRIP-Server, Port, Mountpoint, User, Pass sind hardcoded in `main.cpp`. GNSS-Baudraten, Timeout-Werte und andere Parameter sind ueber den Code verstreut. Es gibt keinen Mechanismus, um Konfiguration zur Runtime zu aendern (via Serial, WebUI, SD-Config). Die Modul-Default-States sind direkt in `modules.cpp` hardcoded.

- **Scope (in)**:
  - **`include/soft_config.h`** erstellen: Namespace `cfg` mit `constexpr` Defaults fuer alle nutzereditierbaren Werte.
    - NTRIP: Host, Port, Mountpoint, User, Pass (alle leer als Default).
    - GNSS: Baudrate, Retry-Timeout.
    - Netzwerk: DHCP an/aus, statische IP.
    - Logging: Intervall, aktiv/inaktiv.
    - Module-Defaults: Welches Modul standardmaessig ON/OFF.
  - **`src/logic/runtime_config.h`** erstellen: `RuntimeConfig` struct (mutable RAM-Kopie).
  - **`src/logic/runtime_config.cpp`** erstellen:
    - `softConfigLoadDefaults()` — kopiert `cfg::*` in `RuntimeConfig`.
    - `softConfigLoadOverrides()` — laedt User-Aenderungen (zunaechst Stub, spaeter SD/Serial/WebUI).
  - **Integration mit modules.cpp**: `MODULE_DEFAULTS` Array wird aus `cfg::` gelesen statt hardcoded.
  - **`main.cpp`**: NTRIP-Config wird aus `cfg::` Defaults geladen statt hardcoded.

- **Nicht-Scope (out)**:
  - Implementierung eines Serial-Command-Parsers fuer Config-Aenderungen (spaeterer Task).
  - SD-Karten-basierte Config-Speicherung (spaeterer Task).
  - WebUI fuer Config (spaeterer Task).
  - NVS-basierte Persistierung (spaeterer Task).

- **files_read**:
  - `include/fw_config.h` (nach TASK-026)
  - `src/logic/modules.cpp` (nach TASK-027, Modul-Defaults)
  - `src/logic/modules.h` (nach TASK-027, ModState)
  - `src/main.cpp` (aktuelle NTRIP-Config, setup())
  - `src/logic/global_state.h`
  - `src/logic/dependency_policy.h`

- **files_write**:
  - `include/soft_config.h` (NEU)
  - `src/logic/runtime_config.h` (NEU)
  - `src/logic/runtime_config.cpp` (NEU)
  - `src/logic/modules.cpp` (MODULE_DEFAULTS → cfg::* Referenzen)
  - `src/main.cpp` (NTRIP-Config aus cfg::*)

- **public_surface**:
  - `include/soft_config.h` — oeffentliche Defaults, von allen Modulen lesbar
  - `src/logic/runtime_config.h` — oeffentliche Runtime-Config-Struct

- **merge_risk_files**:
  - `src/main.cpp` — Entry-Point
  - `src/logic/modules.cpp` — zentrales Modul-System

- **risk_notes**:
  - Soft-Config muss nicht-threadsafe sein (wird nur in setup() und maintTask gelesen). Werden spaeter Serial-Commands hinzugefuegt, muss Race-Condition mit Runtime-Tasks beachtet werden.
  - constexpr Defaults in Header koennen Compile-Zeit erhoehen, wenn Strings zu lang sind.

- **AC**:
  - `include/soft_config.h` existiert mit namespace `cfg` und `constexpr` Defaults fuer NTRIP, GNSS, Netzwerk, Logging, Module-States.
  - `src/logic/runtime_config.h` definiert `RuntimeConfig` struct.
  - `softConfigLoadDefaults()` kopiert alle `cfg::*` Werte in eine `RuntimeConfig` Instanz.
  - `softConfigLoadOverrides()` ist ein Stub der "not yet implemented" zurueckgibt (Platzhalter fuer spaetere SD/Serial/WebUI Implementierung).
  - NTRIP-Config in `main.cpp` verwendet `cfg::NTRIP_*` Defaults statt hardcoded Werte.
  - `pio run` baut fehlerfrei.

- **verification**:
  - `pio run` fuer alle relevanten Environments.
  - Review: keine hardcoded NTRIP-Credentials mehr in `main.cpp`.
  - Build mit geaenderten `cfg::` Defaults → Verhalten aendert sich entsprechend.

- **Links**:
  - `backlog/epics/EPIC-003-platform-and-reuse.md`
  - `backlog/tasks/TASK-026-fw-config-und-board-profile-restrukturierung.md`
