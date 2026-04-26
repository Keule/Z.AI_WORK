# ADR-ZAI-001: Git- und GitHub-Arbeitsweise im Z.AI_WORK Repository

- Status: accepted
- Datum: 2026-06-19
- Autor: Z.ai Agent (Session 2026-06)

---

## Kontext

Das Repository `Keule/Z.AI_WORK` wird über mehrere Chat-Sessions hinweg
von KI-Agenten bearbeitet. Jede Session hat einen begrenzten Kontext
(Context Window), wodurch Zwischenergebnisse verloren gehen können.
Der menschliche Maintainer arbeitet asynchron und muss den Repo-Zustand
nach jeder Session prüfen und freigeben.

Bisher gab es keine dokumentierte Konvention für den Git-Workflow.
Das führte zu:

- **Session-ID-Commits** ohne Semantik (z.B. `c1d34e8d-b538-…`)
- **Fehlende Backup-Strategie** vor Breaking Changes
- **Unklare Branch-Policy** (Feature-Branches vs. main-only)
- **Wiederholte Kontextverluste** bei Session-Übergängen

---

## Entscheidung

### Branch-Strategie: Trunk-Based mit Remote-Backups

| Element | Regel |
|---------|-------|
| **Arbeits-Branch** | Immer `main` |
| **Backup vor Phase** | Remote-Branch anlegen (nur remote, kein lokaler Checkout) |
| **Feature-Branches** | Nur für manuelle PRs des Maintainers, nicht für Agenten |
| **Release-Tags** | Nicht verwendet (ESP32-Firmware wird per OTA/SD verteilt) |

### Backup-Branch vor jeder Phase

```bash
# Vor Beginn einer neuen Phase:
git push origin main:refs/heads/<backup_name>
# Beispiel: git push origin main:refs/heads/main_backup_pre3
```

**Regeln für Backup-Branches:**
- Name-Pattern: `main_backup_pre<N>` oder `backup/<datum_<beschreibung>`
- **Nur remote** — kein lokaler Checkout, kein lokaler Branch nötig
- Dient als Rollback-Punkt, falls eine Phase Fehler einführt
- Bleibt dauerhaft auf GitHub bestehen (wird nicht gelöscht)

### Commit-Konventionen

**Commit-Messages** MUSSSEN folgendes Format haben:

```
<Type>: <Kurze Zusammenfassung>

<Optionale Detailbeschreibung>
```

| Type | Verwendung |
|------|-----------|
| `Phase N:` | Beginn/Abschluss einer Refactoring-Phase |
| `feat(<scope>):` | Neue Funktion |
| `fix(<scope>):` | Bugfix |
| `refactor(<scope>):` | Umbau ohne Verhaltensänderung |
| `docs:` | Dokumentation |
| `chore:` | Wartung (Dependencies, Build-Config) |

**Verboten:**
- Session-IDs als Commit-Message (`c1d34e8d-b538-…`)
- Leere Commit-Messages
- "WIP"-Commits (entweder fertig commiten oder stashen)

### Authentifizierung

- **Personal Access Token (PAT)**: Wird vom Maintainer pro Session bereitgestellt
- Token MUSS nach Session-Ende vom Maintainer rotiert/gesperrt werden
- Token wird NIE im Code, in Config-Dateien oder im ADR gespeichert
- Keine SSH-Keys, kein `gh` CLI — nur `git push`/`pull` mit HTTPS + PAT

### Session-Übergang

Jede Agenten-Session MUSS:

1. **Einstieg**: `git log --oneline -10` lesen, Worklog prüfen, aktuellen Stand verstehen
2. **Vor Arbeit**: Backup-Branch anlegen
3. **Während Arbeit**: Atomare, semantische Commits
4. **Abschluss**: Auf `main` pushen, Zusammenfassung an den Maintainer

### Was funktioniert

| Aktion | Funktioniert | Bemerkung |
|--------|:------------:|-----------|
| `git push origin main` | ✅ | PAT-Auth via HTTPS |
| `git push origin main:refs/heads/<branch>` | ✅ | Remote-Branch erstellen ohne Checkout |
| Direkt auf `main` committen | ✅ | Trunk-based Workflow |
| `git log --oneline -N` | ✅ | Status-Übersicht |
| `git diff --stat` | ✅ | Änderungsübersicht |
| `git add -A && git commit` | ✅ | Atomic commits |

### Was NICHT funktioniert / nicht verfügbar

| Aktion | Status | Grund |
|--------|:------:|-------|
| `pio run` / PlatformIO Build | ❌ | PlatformIO nicht im Sandbox installiert |
| `git merge` / Feature-Branches | ⚠️ | Möglich, aber vom Workflow ausgeschlossen |
| `git rebase -i` | ⚠️ | Interaktive Rebase im Agenten-Chat fehleranfällig |
| `git push --force` | 🔴 | **Verboten** — kann Maintainer-Historie zerstören |
| `gh` CLI / GitHub API | ❌ | Nicht im Sandbox installiert |
| CI/CD Pipeline | ❌ | Kein GitHub Actions Workflow definiert |
| Lokale Build-Verifikation | ❌ | Kein ESP32-Toolchain in der Cloud-Sandbox |
| Pull Requests via CLI | ❌ | `gh` nicht verfügbar |

---

## Invarianten

1. **`main` ist immer deploybar** — Jeder Commit auf main muss den Code in
   einem kompilierbaren Zustand hinterlassen (auch ohne lokale Build-Verifikation).
2. **Kein Force-Push auf `main`** — Geschichte ist immutable.
3. **Backup-Branches werden nie gelöscht** — Sie dienen als dauerhafte
   Referenzpunkte für Rollbacks.
4. **Ein Commit = eine atomare Einheit** — Misch-Commits über mehrere
   konzeptionelle Änderungen vermeiden.
5. **Jeder Commit muss ohne Chat-Kontext verständlich sein** —
   ADR-000 Invariante: "Ein späterer Agent muss ohne Originalchat weiterarbeiten können."

---

## Abgrenzung: Repo-Nutzung durch Maintainer (manuell)

Die oben genannten Regeln gelten für KI-Agenten-Sessions.
Der Maintainer kann abweichend arbeiten:
- Feature-Branches + PRs manuell erstellen
- Lokal bauen und testen vor Push
- Tags für Releases setzen
- `git push --force` auf Nicht-main-Branches

---

## Konsequenzen

### Positiv
- Einfacher Workflow — kein Branch-Management-Overhead für Agenten
- Jeder Commit auf main hat einen Backup-Punkt
- Session-Übergänge sind sicher dank Backup-Branches
- Git-Historie bleibt lesbar und semantisch

### Negativ
- Kein lokales Build-Feedback — Fehler werden erst beim Maintainer-Build sichtbar
- main kann theoretisch in einem nicht-kompilierenden Zustand landen (muss per Code-Review minimiert werden)
- Backup-Branches akkumulieren sich auf GitHub (Speicher: vernachlässigbar)

### Risiken
| Risiko | Wahrscheinlichkeit | Mitigation |
|--------|:-----------------:|-----------|
| Agent pusht kaputten Code auf main | Mittel | Backup-Branch + Maintainer-Review |
| PAT-Leak | Niedrig | Token wird per Session bereitgestellt und danach rotiert |
| Kontextverlust bei Session-Wechsel | Hoch | Worklog + ADR + semantische Commits |

---

## Alternativen

- **Feature-Branches + PRs für Agenten**
  → Zu viel Overhead für Single-Agent-Sessions; `gh` CLI nicht verfügbar.
- **Git-Tags statt Branches für Backups**
  → Tags sind immutable und schwerer zu erstellen/löschen; Branches sind flexibler.
- **Keine Backups**
  → Riskant bei Breaking Changes; Maintainer muss manuell `git reset --hard` machen.

---

## Historie der Backup-Branches

| Branch | Commit | Phase | Datum |
|--------|--------|-------|-------|
| `backup_main` | `d35ec40` | Vor Phase 1+2 Korrekturen | 2026-06 |
| `main_backup_pre3` | `f0cb46f` | Vor Phase 3 (SharedSlot NTRIP) | 2026-06 |
