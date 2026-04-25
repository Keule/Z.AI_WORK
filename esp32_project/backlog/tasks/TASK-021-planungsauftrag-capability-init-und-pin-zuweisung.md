# TASK-021 Planungsauftrag: Capability-Aktivierung, Boot-Init und Pin-Zuweisung strukturieren

- **ID**: TASK-021
- **Titel**: KI-Planer soll in neuem Branch einen Umsetzungsplan für compile-time/runtime Capabilities und Pin-Zuweisung erstellen
- **Status**: done
- **Priorität**: high
- **Komponenten**: `backlog/tasks/`, `backlog/index.yaml`, `src/logic/features.h`, `src/main.cpp`, `src/hal_esp32/*`, `include/hardware_pins.h`, `platformio.ini`
- **Dependencies**: TASK-020
- **Kontext/Problem**:
  - Für modulare Hardwareprofile müssen zusätzliche SPI-Busse/UARTs nur dann aktiviert bzw. initialisiert werden, wenn sie von einem Modul tatsächlich benötigt werden.
  - Aktuell ist die Entkopplung zwischen Compile-Time-Capabilities, Boot-Initialisierung und finaler Pin-Zuweisung nicht als zusammenhängender Planungsauftrag für KI-Entwickler aufgeteilt.
  - Es wird ein dedizierter KI-Planer-Auftrag benötigt, der in einem **neuen Branch** konkrete Entwickler-Tasks erstellt und mit klaren AC/Abhängigkeiten versieht.
- **Scope (in)**:
  - KI-Planer erzeugt in einem neuen Branch drei Entwickler-Tasks mit klarer Reihenfolge, Schnittstellen und Verifikation.
  - Die Entwickler-Tasks decken exakt folgende Themen ab:
    1. (De-)Aktivierbarkeit von Capabilities beim Kompilieren.
    2. (De-)Initialisieren von Capabilities beim Booten.
    3. Pin-Zuweisung beim Initialisieren.
  - Pro Entwickler-Task müssen `files_read`, `files_write`, `risk_notes`, `verification` und `dependencies` enthalten sein.
- **Nicht-Scope (out)**:
  - Implementierung der Firmware-Änderungen selbst.
  - Hardware-Feldtest oder funktionale Endabnahme.
- **AC**:
  - Ein KI-Planer-Branch wurde explizit benannt und für den Planungsauftrag verwendet.
  - Es existieren drei neue, umsetzbare KI-Entwickler-Tasks (je ein Task pro Themenblock 1-3).
  - Die drei Entwickler-Tasks enthalten je klare Akzeptanzkriterien, betroffene Dateien und Test-/Verifikationskommandos.
  - Abhängigkeiten zwischen den drei Aufgaben sind konsistent (Compile-Time -> Boot-Init -> Pin-Zuweisung bzw. begründet alternative Reihenfolge).
  - `backlog/index.yaml` und relevante Epic-Referenzen sind konsistent aktualisiert.
- **Verifikation/Test**:
  - Struktureller Review der erzeugten Task-Dateien gegen `backlog/README.md`.
  - Optional: `python3 tools/validate_backlog_index.py`.
- **Owner**: ki-planer
- **Links**:
  - `backlog/README.md`
  - `backlog/epics/EPIC-003-platform-and-reuse.md`
  - `backlog/tasks/TASK-020-tft-espi-und-lilygo-eth-lite-treiber-integration-planen.md`
- **delivery_mode**: firmware_only
- **task_category**: platform_reuse
