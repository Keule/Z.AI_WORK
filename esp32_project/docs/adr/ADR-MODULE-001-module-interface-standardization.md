# ADR-MODULE-001: Module Interface Standardization

- Status: accepted
- Datum: 2026-04-23

## Kontext

Mit dem Feature-Modulsystem existiert bereits eine einheitliche Runtime-Aktivierung,
aber die hardware-nahen Module nutzen teils heterogene Lebenszyklus- und
Health-Schnittstellen. Das erschwert:

- zentrale Orchestrierung (Init/Update-Reihenfolge),
- konsistente Diagnose und Telemetrie,
- klare Abnahmekriterien für Phase 4.

Zusätzlich benötigen reine Service-Module (z. B. CLI, reine Diagnostik) nicht
den gleichen Hardware-Vertrag und dürfen nicht unnötig überreguliert werden.

## Entscheidung

Für **hardware-nahe Module** wird ein verpflichtender, einheitlicher Vertrag
definiert. Betroffene Module:

- `control`
- `imu`
- `net`
- `actuator`
- `steer_angle`
- `sd_logger`

Normative Referenz für die zentrale Registry bleibt:

- `src/logic/module_interface.h`

Die dortige Schnittstelle wird für obige Module nicht mehr als „optional“,
sondern als **Pflichtvertrag** interpretiert.

## Soll-Signaturen (Pflicht)

Für jedes hardware-nahe Modul gelten folgende Soll-Signaturen:

```cpp
bool <module>IsEnabled();
void <module>Init();
bool <module>Update();
bool <module>IsHealthy(uint32_t now_ms);
```

Zusätzlich führt jedes hardware-nahe Modul intern einen standardisierten
`ModState`-Datensatz mit mindestens diesen Feldern:

```cpp
struct ModState {
    bool     detected;       // Hardware/Abhängigkeit erkannt
    bool     quality_ok;     // Daten-/Funktionsqualität innerhalb Grenzwerte
    uint32_t last_update_ms; // Zeitstempel der letzten erfolgreichen Aktualisierung
    int32_t  error_code;     // 0 = OK, !=0 = modulspezifischer Fehlercode
};
```

### Semantik der ModState-Felder

- `detected`
  - `true`, wenn Modulhardware (bzw. zwingende Vorbedingung) erkannt ist.
  - `false` verhindert `healthy=true`.
- `quality_ok`
  - `true`, wenn modulinterne Plausibilitäts-/Qualitätskriterien erfüllt sind.
- `last_update_ms`
  - muss bei jedem erfolgreichen `Update()` monotonic aktualisiert werden.
- `error_code`
  - `0` bei fehlerfreiem Zustand,
  - stabiler, dokumentierter Nicht-Null-Code bei Fehlern.

`<module>IsHealthy(now_ms)` ist genau dann `true`, wenn mindestens gilt:

- `detected == true`
- `quality_ok == true`
- `(now_ms - last_update_ms) <= module_freshness_timeout_ms`
- `error_code == 0`

## Ausnahmen (reine Service-Module)

Module ohne direkte Hardware-/Sensor-/Aktor-Verantwortung (z. B. CLI,
textuelle Diagnostik, reine Protokoll-Utilities) sind von der Pflicht auf
`detected` und hardwarebezogene Freshness-Semantik ausgenommen.

Für solche Service-Module gilt:

- sie **dürfen** weiterhin über `ModuleOps` registriert werden,
- sie **müssen nicht** den vollständigen `ModState`-Hardwarevertrag führen,
- sie **müssen** klar als `service` dokumentiert sein und dürfen keine
  Hardware-Gesundheit vortäuschen.

## Invarianten

- Hardware-nahe Module ohne die vier Pflichtfunktionen gelten als nicht
  Phase-4-abnahmefähig.
- `IsHealthy()` darf keine Seiteneffekte auf den Modulzustand haben.
- `error_code`-Semantik muss je Modul dokumentiert und stabil sein.
- Service-Module dürfen den Hardware-Health-Kanal nicht „grün“ melden.

## DoD-Kriterien (messbar) für Phase-4-Abnahme

Ein Build erfüllt diese ADR in Phase 4 nur, wenn alle Kriterien erfüllt sind:

1. **API-Vollständigkeit**
   - Für jedes der sechs Pflichtmodule existieren die vier Soll-Signaturen
     kompilierbar und verlinkbar.
   - Nachweis: Build-Log + symbolische Referenzprüfung der Modulregistry.

2. **State-Vollständigkeit**
   - Jedes Pflichtmodul führt einen `ModState` mit den Feldern
     `detected`, `quality_ok`, `last_update_ms`, `error_code`.
   - Nachweis: Code-Review-Checkliste, 6/6 Module positiv.

3. **Health-Konsistenz**
   - Bei injiziertem Fehler (`error_code != 0`) wechselt `IsHealthy()` innerhalb
     eines Update-Zyklus auf `false`.
   - Nachweis: automatisierter Test oder reproduzierbarer HIL-Testfall, 6/6 Module.

4. **Freshness-Verhalten**
   - Bei ausbleibendem `Update()` wird nach Ablauf von
     `module_freshness_timeout_ms` deterministisch `healthy=false`.
   - Nachweis: Zeitbasierter Test mit dokumentiertem Timeout je Modul.

5. **Service-Ausnahme sauber dokumentiert**
   - Alle nicht-hardware-nahen registrierten Module sind als Service markiert
     und begründen die Ausnahme.
   - Nachweis: Architekturdokument + Modulübersicht ohne offene Punkte.

## Konsequenzen

### Positiv

- Einheitliche Integrations- und Testbarkeit hardware-naher Module.
- Eindeutige, vergleichbare Health-Aussagen über alle Kernmodule.
- Klare Abnahmekriterien für Phase 4 ohne Interpretationsspielraum.

### Negativ

- Initialer Refactoring-Aufwand pro Modul.
- Striktere Kontrakte erhöhen kurzfristig den Anpassungsdruck bei Legacy-Code.

## Alternativen

- Status quo mit modulindividuellen Interfaces
  - verworfen: erschwert Orchestrierung und objektive Abnahme.
- Vertrag nur über Konvention, ohne messbare DoD-Kriterien
  - verworfen: zu interpretationsanfällig für Phase-4-Gates.

## Plan-Integration

Diese ADR wird in Schritt 7 (Code-Aufteilung und HAL-Split) umgesetzt. Die Module-Interface-Standardisierung ist Voraussetzung für die CLI-Modularisierung.

