# P1 (dependent): API-Freeze + Discovery-vs-Core CRC-Grenze

**Status:** Planbar, aber **startbar erst nach Exklusiv-Gate**.  
**Priorität:** P1

## Ziel
Vor der parallelen Umsetzung wird eine unveränderliche Architekturgrenze festgelegt:

1. **API-Freeze** für öffentliche Schnittstellen (Header, Zielstruktur, Kompatibilität).
2. **Discovery-vs-Core CRC-Regeln** als feste Boundary im Protokollpfad.

Danach können die technischen Refactorings parallel laufen.

---

## Exklusiv-Gate (nur 1 Bearbeiter)

### 1) API-Freeze festlegen

**Scope (öffentliche API):**
- `src/logic/pgn_types.h`
- `src/logic/pgn_codec.h`
- `src/logic/pgn_registry.h`
- `src/logic/net.h`

**Freeze-Regeln:**
- Keine breaking Renames in exported Typen/Funktionen.
- Signaturen bleiben binär/semantisch stabil; Erweiterungen nur additiv.
- Header-Includes werden auf minimale, reproduzierbare Abhängigkeiten begrenzt.
- Zielstruktur wird als Modulgrenze dokumentiert (`types`, `codec`, `registry`, `net`).

**Definition of Done:**
- API-Liste (Funktionen, Structs, Konstanten) ist dokumentiert.
- Für jeden späteren API-Bruch ist explizite ADR/Versionierungsentscheidung nötig.

### 2) Discovery-vs-Core CRC-Architekturgrenze fixieren

**Unveränderliche Regel:**
- **Discovery/Management-PGNs** (z. B. 200/201/202 aus AgIO-Pfad) dürfen den Discovery-Sonderpfad nutzen.
- **Core-AOG-PGNs** bleiben strikt checksummiert/validiert.
- Ein zukünftiger Fix im Core-Pfad darf den Discovery-Pfad nicht implizit ändern (und umgekehrt).

**Technische Leitplanken:**
- Eine zentrale Entscheidungsstelle (z. B. `pgnIsDiscovery(...)`) bleibt der einzige Umschaltpunkt.
- Tests müssen beide Pfade getrennt absichern (Discovery tolerant, Core strikt).

**Definition of Done:**
- Boundary-Regel dokumentiert und als "nicht verhandelbar" markiert.
- Änderungen an CRC-Validierung ohne Anpassung beider Testklassen sind nicht mergebar.

---

## Danach parallel ausführbar

### 3) Subtask A: Types/Codec extrahieren

**Inhalt:**
- Extraktion/Kapselung von Typen + Codec in klar getrennte Einheit.
- Öffentliche API bleibt gemäß Freeze unverändert.
- Interne Includes und Build-Abhängigkeiten reduzieren.

**Akzeptanzkriterien:**
- Build grün.
- Codec-Selftests grün.
- Keine API-Breaks in Konsumenten.

### 4) Subtask B: Registry/Net auf neue API umstellen

**Inhalt:**
- `registry` und `net` auf die eingefrorene Types/Codec-API umbauen.
- Kein Umweg über alte interne Hilfsroutinen.

**Akzeptanzkriterien:**
- Discovery-Flow funktionsfähig.
- Core-PGN-Validierung unverändert strikt.
- Smoke-Tests für Discovery/Hello/Subnet und Core-Pfade grün.

---

## Empfohlene Reihenfolge / Ownership

- **Gate Owner (1 Person):** Exklusiv-Gate 1+2 finalisieren und mergen.
- **Danach 2 Streams parallel:**
  - Stream A: Subtask A
  - Stream B: Subtask B
- **Integrations-Checkpoint:** gemeinsamer Rebase + Testmatrix vor finalem Merge.

## Risiko- und Merge-Hinweise

- Höchstes Risiko: implizite Verhaltensänderung an CRC-Entscheidungslogik.
- Deshalb: kleine PRs, frühe Integrationsläufe, klare Testpflicht pro Pfad.
- Bei Konflikten zwischen Refactoring und API-Freeze gilt immer der Freeze (bis ADR-Entscheid).

---

## Dokumentations-Templates (Standard)

Für Session- und Entscheidungsdokumentation sind folgende Vorlagen zu verwenden:

- [`templates/session-start.md`](../../templates/session-start.md)
- [`templates/session-progress.md`](../../templates/session-progress.md)
- [`templates/session-handover.md`](../../templates/session-handover.md)
- [`templates/task.md`](../../templates/task.md)
- [`templates/adr.md`](../../templates/adr.md)
