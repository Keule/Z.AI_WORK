# ADR-001: Config-Schichtung in fw_config, soft_config und RuntimeConfig

- Status: accepted
- Datum: 2026-04-20

## Kontext

Im Projekt werden Hardwarebelegung, compile-time Defaults und zur Laufzeit
veränderliche Parameter benötigt. Wenn diese Schichten vermischt werden,
entstehen Fehlinterpretationen und schwer testbare Seiteneffekte.

## Entscheidung

Die Konfiguration ist in drei Schichten zu trennen:

1. **`fw_config`**
   - Board-/Firmware-nahe Konfiguration
   - Hardwareprofile
   - compile-time feste technische Zuordnung

2. **`soft_config`**
   - compile-time Nutzer-/Projektdefaults
   - fachliche Defaultwerte
   - keine unmittelbare Hardware-Pin-Source-of-truth

3. **`RuntimeConfig`**
   - mutable RAM-Kopie
   - Laufzeitkonfiguration
   - aus Defaults geladen, später überschreibbar

## Invarianten

- Hardwarezuordnung gehört nicht in `soft_config`.
- Laufzeitänderungen dürfen `fw_config` nicht implizit überschreiben.
- Beispiel-/Demo-Werte dürfen nicht stillschweigend produktive Defaults ersetzen.

## Konsequenzen

### Positiv
- saubere Trennung von Hardware, Defaultwerten und Laufzeitverhalten
- besser testbar
- klarere Zuständigkeiten

### Negativ
- mehr Dateien und Übergänge
- höhere Disziplin bei Initialisierung notwendig

## Alternativen

- eine einzige globale Config-Struktur  
  → zu unscharf, mischt Ebenen.
- reine compile-time Konfiguration ohne Runtime-Kopie  
  → unflexibel für spätere UI-/Serial-/SD-Overrides.
