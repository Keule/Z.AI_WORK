# Task: NTRIP hwStatusSetFlag() Bug beheben — GNSS-Status fälschlich als Fehler (Review-F2)

- **Origin:** Kombinierter Review TASK-026..030, Finding F2 (Kritisch)
- **Entscheidung:** Klare Code-Korrektur, keine Architekturvariante

- **Status:** done

## Kontext / Problem

In `ntrip.cpp` Zeile 429 ruft der `NtripConnState::CONNECTED`-Zweig in `ntripTick()` folgende Funktion auf:

```cpp
hwStatusSetFlag(HW_GNSS, HW_SEV_OK);
```

`hwStatusSetFlag()` in `hw_status.cpp` (Zeile 76-85) setzt intern **immer** `error = true`, unabhängig von der übergebenen Severity:

```cpp
void hwStatusSetFlag(HwSubsystem id, HwSeverity severity) {
    if (id >= HW_COUNT) return;
    if (!s_subsys[id].error) {
        s_subsys[id].error = true;      // ← IMMER true!
        s_subsys[id].severity = severity;
        s_subsys[id].first_seen = hal_millis();
        LOGE("HWS", "ERROR flagged: %s (sev=%u)", ...);
    }
}
```

**Folge:** Eine erfolgreiche NTRIP-Verbindung wird als GNSS-Fehler registriert. Der TASK-030 AC *"NTRIP-Status connected/disconnected/error wird berücksichtigt"* ist semantisch nicht korrekt umgesetzt. Im `hwStatusUpdate()`-Pfad (`hw_status.cpp` Zeile 235-237) wird `HW_GNSS` nur dann gelöscht, wenn NTRIP inaktiv ist — bei aktiver, erfolgreicher NTRIP-Verbindung bleibt der Fehler-Flag gesetzt.

Der Fehler wurde **nur vom Codex-Review gefunden** (Super Z Review hatte ihn übersehen). Dies unterstreicht den Wert des Dual-Review-Ansatzes.

## Akzeptanzkriterien

1. Im `CONNECTED`-Zustand von `ntripTick()` wird `HW_GNSS` **nicht** als Fehler markiert
2. Im `ERROR`- und `DISCONNECTED`-Zustand wird `HW_GNSS` weiterhin als Warning/Error markiert (bestehendes Verhalten)
3. `hwStatusUpdate()` zeigt korrekten GNSS-Status (kein persistentes false-positive)
4. TASK-030 AC erfüllt: *"NTRIP-Status connected/disconnected/error wird berücksichtigt"*
5. Semantische Korrektheit: Severity `HW_SEV_OK` bedeutet "kein Fehler", nicht "Fehler mit OK-Schwere"

## Scope (in)

- `src/logic/ntrip.cpp` Zeile 429: CONNECTED-Zweig korrigieren

### Empfohlene Lösung

**Option A (minimaler Eingriff):** CONNECTED-Zweig ändert von `hwStatusSetFlag()` auf `hwStatusClearFlag()`:

```cpp
// Vorher (falsch):
hwStatusSetFlag(HW_GNSS, HW_SEV_OK);

// Nachher (korrekt):
hwStatusClearFlag(HW_GNSS);
```

**Option B (allgemeiner Fix):** `hwStatusSetFlag()` so anpassen, dass `HW_SEV_OK` den error-Flag nicht setzt. Dies hat breitere Wirkung und verhindert ähnliche Bugs in Zukunft:

```cpp
void hwStatusSetFlag(HwSubsystem id, HwSeverity severity) {
    if (id >= HW_COUNT) return;
    if (severity == HW_SEV_OK) {
        // OK severity = clear error, not set error
        hwStatusClearFlag(id);
        return;
    }
    if (!s_subsys[id].error) {
        s_subsys[id].error = true;
        s_subsys[id].severity = severity;
        s_subsys[id].first_seen = hal_millis();
        LOGE("HWS", "ERROR flagged: %s (sev=%u)", ...);
    }
}
```

**Empfehlung Planer:** Option B ist robuster, hat aber breitere Wirkung. Der Entwickler soll Option B bevorzugen, sofern keine anderen Aufrufer von `hwStatusSetFlag(HW_*, HW_SEV_OK)` existieren, die sich auf das aktuelle Verhalten verlassen (Tool: `rg "hwStatusSetFlag.*HW_SEV_OK"` prüfen).

## Nicht-Scope (out)

- Keine Änderung am `hwStatusUpdate()`-Pfad in `hw_status.cpp` (bestehendes Clearing-Verhalten ist korrekt)
- Keine Änderung an `ERROR`- oder `DISCONNECTED`-Zweigen in `ntripTick()` (dort ist `HW_SEV_WARNING` korrekt)

## Verifikation / Test

- `pio run -e T-ETH-Lite-ESP32S3` (profile_full_steer) — muss kompilieren
- `pio run -e T-ETH-Lite-ESP32` — muss kompilieren
- Code Review: `hwStatusSetFlag()` wird nirgends mit `HW_SEV_OK` für "beabsichtigten Fehler" aufgerufen
- `rg "hwStatusSetFlag.*HW_SEV_OK" src/` — prüfen ob weitere Aufrufer existieren

## Relevante ADRs

- **ADR-GNSS-001** (NTRIP-Policy): *"NTRIP-Status muss mit GNSS-/HW-Status konsistent modelliert werden."* → Wird durch diesen Fix hergestellt.

## Invarianten

- `hwStatusClearFlag()` existiert bereits und funktioniert korrekt (setzt `error=false`, `severity=HW_SEV_OK`)
- `hwStatusHasError(HW_GNSS)` muss bei erfolgreicher NTRIP-Verbindung `false` zurückgeben

## Known Traps

1. **Nur ein Review hat den Bug gefunden.** Es könnten weitere ähnliche semantische Probleme in anderen Zustandsautomaten existieren. Der Entwickler sollte alle `hwStatusSetFlag()`-Aufrufe auf Korrektheit prüfen.
2. **`hwStatusUpdate()` Cleared GNSS nur wenn NTRIP inaktiv:** In `hw_status.cpp` Zeile 235-237 wird `HW_GNSS` nur gelöscht, wenn `!ntrip_active`. Wenn NTRIP aktiv UND verbunden ist, wird `hwStatusUpdate()` den Flag nicht anfassen — das ist korrekt, da `ntripTick()` im CONNECTED-Zustand den Status setzt.

## Merge Risk

- **Niedrig:** Lokaler Change in einem if-Zweig von `ntripTick()`. Ggf. minimaler Change in `hwStatusSetFlag()`.

## Classification

- **category:** runtime_stability
- **priority:** high
- **delivery_mode:** firmware_only
- **exclusive_before:** Keine
- **parallelizable_after:** Parallel mit allen anderen TASK-03x

## Owner

- **Assigned:** KI-Entwickler
- **Status:** todo
