# Session Handover

## Zweck
Übergabe an die nächste Person/Session mit klarem Ist-Stand und konkretem Next Step.

## Minimalpflichtfelder
- **Datum/Zeit (UTC):**
- **Status (Ampel):** `grün | gelb | rot`
- **Was wurde abgeschlossen:**
- **Offene Punkte:**
- **Nächster konkreter Schritt:**
- **Referenzen (PR/Commits/Dateien):**

## Vorlage
```md
# Session Handover
- Datum/Zeit (UTC):
- Status (Ampel):
- Was wurde abgeschlossen:
- Offene Punkte:
- Nächster konkreter Schritt:
- Referenzen (PR/Commits/Dateien):
```

## Beispiel
```md
# Session Handover
- Datum/Zeit (UTC): 2026-04-17 12:05
- Status (Ampel): gelb
- Was wurde abgeschlossen: Template-Dateien angelegt, README + Prozessdoku verlinkt.
- Offene Punkte: Teamfeedback zu Pflichtfeldern einarbeiten.
- Nächster konkreter Schritt: Feld `Entscheidung` im ADR-Template validieren und ggf. erweitern.
- Referenzen (PR/Commits/Dateien): PR #77, `templates/*.md`, `README.md`, `docs/plans/P1-*.md`
```
