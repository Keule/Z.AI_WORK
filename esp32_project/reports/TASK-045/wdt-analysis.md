# TASK-045: Task-Watchdog-Reset auf ESP32 Classic — Analyse

**Datum**: 2026-04-23 (aktualisiert 2026-07-07)
**Status**: Analyse Phase 1 — teilweise mitigiert
**Komponenten**: hal_impl.cpp, sd_logger_esp32.cpp, main.cpp, ntrip.cpp, sdkconfig.defaults

## Zusammenfassung

Der WDT-Reset tritt auf dem ESP32 Classic (LilyGO T-ETH-Lite-ESP32) nach ~30 s auf.
Ursache: IDLE-Task auf CPU 0 verhungert durch ETH DMA-IRQ-Last (RMII intern) in
Kombination mit commTask Prioritaet.

## WDT-Ausloeser-Analyse

### 1. ESP32 Classic RMII Ethernet (Primärer Verdacht)

- ESP32 Classic nutzt internen EMAC mit RMII PHY (RTL8201)
- ETH DMA fuehrt IRQ-Verarbeitung auf Core 0 durch
- Bei Netzwerklast kann die IRQ-Behandlung den IDLE-Task verdraengen
- Der ESP32-S3 nutzt W5500 (SPI), hat dieses Problem **nicht**
- **Fazit**: Dies ist eine bekannte Limitation des ESP32 Classic, nicht des S3

### 2. maintTask-Erstellung (Sekundaerer Faktor)

**Aktuelle Logik** (main.cpp Zeile 1008):
```cpp
if (moduleIsActive(MOD_SD) || moduleIsActive(MOD_NTRIP)) {
    sdLoggerMaintInit();
}
```

**Analyse**:
- Wenn `FEAT_NTRIP` kompiliert ist und `moduleActivate(MOD_NTRIP)` erfolgreich war
  → maintTask wird erstellt → ntripTick() laeuft im maintTask (OK per ADR-002)
- Wenn `FEAT_NTRIP` kompiliert ist aber NTRIP nicht aktiviert (host leer)
  → maintTask wird **nicht** erstellt → ntripTick() wird nirgends aufgerufen
- Auf dem ESP32-Classic mit `profile_full_steer`:
  - `feat::act()` ist `false` (keine ACT-Pins verdrahtet)
  - MOD_SD koennte inaktiv sein (keine SD-Karte)
  - MOD_NTRIP: Haengt vom Profil ab

**Bewertung**: Die maintTask-Erstellung ist korrekt, solange NTRIP aktiviert ist.
Wenn NTRIP inaktiv ist, gibt es kein Blocking-Problem (ntripTick wird nicht aufgerufen).

### 3. WDT-Taskname-Anomalie

Der WDT-Report meldet `"CPU 0: maint"`, obwohl der maintTask ggf. nicht erstellt wurde.
**Erklaerung**: FreeRTOS WDT meldet den Tasknamen zum Zeitpunkt der Registrierung.
Ein Task der geloescht wurde (oder nie erstellt) kann im WDT-Report erscheinen
wenn der Name-Cache nicht aktualisiert wurde. Dies ist ein bekanntes FreeRTOS-Verhalten
und **kein** Indiz fuer einen tatsaechlichen maintTask.

### 4. hal_tcp_connect Blocking-Pfad

In `hal_impl.cpp` Zeile 860:
```cpp
s_tcp_client.setTimeout(5000);  // 5s connect timeout
const bool ok = s_tcp_client.connect(host, port);
```

- 5s Blocking ist nur ein Problem wenn `ntripTick()` ausserhalb des maintTask laeuft
- Per ADR-002 und TASK-029: ntripTick() gehoert in maintTask (blocking OK dort)
- **Bewertung**: Kein Problem, wenn maintTask korrekt erstellt wurde

### 5. ETH Init — Blocking-Pfad in setup()

In `hal_impl.cpp` Zeile 1812-1818:
```cpp
uint32_t wait_start = millis();
while (!s_eth_has_ip && (millis() - wait_start < 5000)) {
    delay(100);
    yield();
}
```

- 5s blocking wait fuer ETH Link-Up/IP im setup() vor Task-Erstellung
- Dies ist **nicht** ein WDT-Problem da tasks noch nicht laufen
- **Bewertung**: OK, aber delay(100) blockiert WiFi event loop — yield() mildert es

### 6. commTask — Nicht-Blocking, aber ETH-DMA-lastig

In `main.cpp` Zeile 1041-1049:
```cpp
xTaskCreatePinnedToCore(
    commTaskFunc, "comm", 4096, nullptr,
    configMAX_PRIORITIES - 3,  // slightly lower priority
    &s_comm_task_handle, 0    // Core 0
);
```

- commTask laeuft auf Core 0 mit priority `configMAX_PRIORITIES - 3`
- Enthaelt `netPollReceive()`, `netSendAogFrames()`, `ntripReadRtcm()`, `ntripForwardRtcm()`
- `ntripTick()` wurde bereits aus dem commTask entfernt (TASK-029)
- **Alle Pfade sind non-blocking** — kein vTaskDelay > 10ms
- **Bewertung**: commTask ist korrekt, ETH DMA IRQ ist der eigentliche WDT-Ausloeser

### 7. WDT-Reset im Arduino loop()

In `main.cpp` Zeile 1069:
```cpp
void loop() {
    // ...
    esp_task_wdt_reset();  // Feed watchdog
}
```

- `loop()` laeuft auf Core 1 (Arduino task) und speist den WDT
- **Aber**: Der Task-Watchdog meldet "IDLE0" — das heisst IDLE-Task auf Core 0
- Der Arduino loop() WDT-Feed hilft nur fuer den Arduino-Task, nicht fuer den IDLE-Task
- **Bewertung**: Korrekt implementiert, aber adressiert nicht den ESP32 Classic Core 0 IDLE-Verhungern

## Bereits umgesetzte Massnahmen

### 1. WDT Timeout erhoet (✓ erledigt)

`sdkconfig.defaults` Zeile 22:
```
CONFIG_ESP_TASK_WDT_TIMEOUT_S=15
```

Default war 5s, jetzt 15s. Dies gibt dem IDLE-Task mehr Puffer fuer ETH DMA Bursts.

### 2. maintTask-Bedingung korrigiert (✓ erledigt)

main.cpp Zeile 1008:
```cpp
if (moduleIsActive(MOD_SD) || moduleIsActive(MOD_NTRIP)) {
    sdLoggerMaintInit();
}
```

Vorher war die Bedingung `feat::act() && feat::safety() && moduleIsActive(MOD_SD)`,
was auf dem ESP32-Classic (feat::act()=false) verhinderte dass der maintTask erstellt wurde.

### 3. ntripTick() aus commTask entfernt (✓ erledigt)

`ntripTick()` laeuft ausschliesslich im maintTask (sd_logger_esp32.cpp Zeile 410).
Im commTask wird nur `ntripReadRtcm()` und `ntripForwardRtcm()` aufgerufen.

### 4. TASK-045 Kommentar in main.cpp (✓ erledigt)

Ein Hinweis-Kommentar (Zeilen 991-996) dokumentiert die WDT-Problematik und
die Notwendigkeit der maintTask-Erstellung fuer NTRIP.

## Verbleibende Risiken

1. **ETH DMA IRQ-Dominanz auf Core 0**: Die 15s Timeout-Erhoehung ist ein
   Workaround, keine Loesung. Bei sehr hoher Netzwerklast kann der IDLE-Task
   weiterhin verhungern. Die einzige vollstaendige Loesung waere
   `esp_task_wdt_reset()` im commTask.

2. **WDT Panic behalten**: `CONFIG_ESP_TASK_WDT_PANIC=y` ist korrekt — der
   Panic-Stack-Trace liefert wertvolle Diagnose-Daten. Nur Timeout erhoehen,
   nicht Panic deaktivieren.

3. **ESP32 Classic vs S3**: Die sdkconfig.defaults gilt fuer beide Targets.
   Eine ESP32-Classic-spezifische WDT-Zeit erfordert separate sdkconfig-Dateien
   (z.B. `sdkconfig_classic.defaults`).

## Empfohlene naechste Schritte

1. **esp_task_wdt_reset() im commTask hinzufuegen**:
   ```cpp
   // commTaskFunc() — nach vTaskDelayUntil()
   esp_task_wdt_reset();
   ```
   Schadet nicht auf ESP32-S3 und hilft auf ESP32-Classic.

2. **Hardware-Validierung**:
   - Firmware mit 15s Timeout auf ESP32-Classic flashen
   - Mindestens 5 Minuten unter Last betreiben
   - Erfolgs-/Misserfolgs-Report in diesem Verzeichnis ablegen

3. **Basis-Vergleich**:
   - `T-ETH-Lite-ESP32` Env (nur ETH, kein NTRIP) auf Classic flashen
   - Pruefen ob WDT auch ohne NTRIP auftritt → isoliert ETH DMA vs. Feature-spezifisch

## Offene Punkte

- Hardware-Test mit 15s Timeout steht aus (hardware_required)
- Basis-Vergleich mit `T-ETH-Lite-ESP32` steht aus
- commTask WDT-Feed steht aus (nur Code-Aenderung, kein Hardware-Test noetig)
- ESP32-Classic-spezifische sdkconfig Pruefung steht aus

## Referenzen

- ADR-002: Task-Modell (commTask darf nicht blockieren)
- ADR-GNSS-001: NTRIP Single-Base Policy
- ADR-LOG-001: Logging/SD Flush Policy
- TASK-029: maintTask fuer blocking Ops
- TASK-005: Externer Hardware-Watchdog (spaeter)
