/**
 * @file op_mode.cpp
 * @brief Operating Mode State Machine (ADR-005)
 *
 * Implementiert die BOOTING → ACTIVE ↔ PAUSED Zustandsmaschine.
 * Verwendet std::atomic<OpMode> fuer lock-freie Lesen aus allen Tasks.
 * Schreibzugriffe (Uebergang) sind durch einen Mutex geschuetzt.
 *
 * Features:
 * - Enhanced Safety Logic mit detaillierten Uebergangsbedingungen
 * - AgIO Notification (PGN 253 Status-Bit + angle/pwm nullung)
 * - GPIO Switch Support mit Debounce auf Safety-Pin GPIO 4 (Step 4)
 * - NVS Persistenz fuer Mode-Praeferenz (ADR-005)
 * - Uebergangs-Mutex gegen konkurrierende Transitions
 * - Sanity-Check fuer PAUSED→ACTIVE: ETH + Pipeline + Link (Step 4)
 */

#include "op_mode.h"
#include "global_state.h"
#include "hal/hal.h"
#include "modules.h"
#include "fw_config.h"

#include "log_config.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_MAIN
#include "esp_log.h"
#include "log_ext.h"

#include <atomic>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#endif

// ===================================================================
// NVS Persistenz — Namespace und Schluessel (ADR-005)
// ===================================================================
static constexpr const char* NVS_OP_MODE_NS  = "agsteer";
static constexpr const char* NVS_OP_MODE_KEY  = "op_mode_pref";

// ===================================================================
// Atomarer Modus-Zustand (lock-free lesen aus allen Tasks)
// ===================================================================
static std::atomic<uint8_t> s_op_mode{static_cast<uint8_t>(OP_MODE_BOOTING)};

// ===================================================================
// Uebergangs-Mutex (verhindert konkurrierende Transitions)
// ===================================================================
static SemaphoreHandle_t s_transition_mutex = nullptr;

// ===================================================================
// Logging Tag
// ===================================================================
static const char* TAG = "OPMODE";

// ===================================================================
// Konstanten
// ===================================================================
static constexpr float MIN_STEER_SPEED_KMH = 0.1f;

/// PGN 253 switchStatus Bit: Device in PAUSED mode
static constexpr uint8_t SWITCH_STATUS_BIT_PAUSED = 0x40;

/// GPIO Pin fuer Mode-Switch (Safety-Pin, ADR-005)
#ifndef SAFETY_IN
#define SAFETY_IN 4  // Fallback wenn nicht im Board-Profile definiert
#endif

/// Anzahl aufeinanderfolgender gleicher Readings fuer Debounce
/// maintTask laeuft alle 1s → 2 Readings = 2s Debounce
static constexpr uint8_t GPIO_DEBOUNCE_COUNT = 2;

// ===================================================================
// GPIO Debounce State
// ===================================================================
static bool s_gpio_last_raw = false;           ///< Letzter Rohwert des Safety-Pins
static uint8_t s_gpio_stable_count = 0;        ///< Zaehler fuer stabile Readings
static bool s_gpio_debounced_state = false;    ///< Entprellter Zustand (true = HIGH/OK)
static bool s_gpio_mode_switch_enabled = false;

// ===================================================================
// Hilfsfunktionen
// ===================================================================

static inline OpMode atomicLoadMode(void) {
    return static_cast<OpMode>(s_op_mode.load(std::memory_order_acquire));
}

static inline void atomicStoreMode(OpMode mode) {
    s_op_mode.store(static_cast<uint8_t>(mode), std::memory_order_release);
}

static void lockTransition(void) {
    if (s_transition_mutex) {
        xSemaphoreTake(s_transition_mutex, portMAX_DELAY);
    }
}

static void unlockTransition(void) {
    if (s_transition_mutex) {
        xSemaphoreGive(s_transition_mutex);
    }
}

/// Setze g_nav.sw.paused Flag (unter StateLock)
static void setPausedFlag(bool paused) {
    StateLock lock;
    g_nav.sw.paused = paused;
}

/// Sanity-Check fuer PAUSED → ACTIVE: ETH + Pipeline + Link (Step 4)
static bool sanityCheckForActive(void) {
    // 1. ETH Modul muss aktiv sein
    if (!moduleIsActive(MOD_ETH)) {
        hal_log("[OPMODE] PAUSED→ACTIVE verweigert: ETH Modul nicht aktiv");
        return false;
    }

    // 2. ETH Link muss UP sein
    if (!hal_net_is_connected()) {
        hal_log("[OPMODE] PAUSED→ACTIVE verweigert: ETH Link DOWN");
        return false;
    }

    // 3. Control-Pipeline muss bereit sein (IMU + ADS + ACT)
    char pipeline_reason[64] = {0};
    if (!moduleControlPipelineReady(pipeline_reason, sizeof(pipeline_reason))) {
        hal_log("[OPMODE] PAUSED→ACTIVE verweigert: Pipeline nicht bereit (%s)",
                pipeline_reason[0] ? pipeline_reason : "unbekannt");
        return false;
    }

    return true;
}

// ===================================================================
// OpMode Public API
// ===================================================================

void opModeInit(void) {
    // Mutex erstellen
    if (!s_transition_mutex) {
        s_transition_mutex = xSemaphoreCreateMutex();
    }

    // Atomaren Zustand zuruecksetzen
    atomicStoreMode(OP_MODE_BOOTING);
    s_gpio_last_raw = false;
    s_gpio_stable_count = 0;
    s_gpio_debounced_state = false;
    s_gpio_mode_switch_enabled = false;

    // NVS-Praeferenz laden
    const OpMode pref = opModeLoadPreference();
    (void)pref;  // Praeferenz wird erst bei opModeBootComplete() beruecksichtigt

    hal_log("[OPMODE] initialisiert (BOOTING, NVS praeferez=%s)", opModeToString(pref));
}

OpMode opModeGet(void) {
    return atomicLoadMode();
}

bool opModeRequest(OpMode target) {
    const OpMode current = atomicLoadMode();

    // Gleicher Modus — kein Uebergang noetig
    if (target == current) {
        hal_log("[OPMODE] bereits in Modus %s", opModeToString(current));
        return false;
    }

    // Uebergang nach BOOTING nie erlaubt
    if (target == OP_MODE_BOOTING) {
        hal_log("[OPMODE] Uebergang nach BOOTING nicht erlaubt");
        return false;
    }

    lockTransition();

    // Re-check nach Lock (konkurrierender Transition moeglich)
    const OpMode actual = atomicLoadMode();
    if (actual != current) {
        unlockTransition();
        hal_log("[OPMODE] Uebergang %s → %s verweigert: Modus hat sich auf %s geaendert",
                opModeToString(current), opModeToString(target), opModeToString(actual));
        return false;
    }

    if (target == OP_MODE_ACTIVE && actual == OP_MODE_PAUSED) {
        // PAUSED → ACTIVE: Sanity-Check (ETH + Pipeline + Link)
        if (!sanityCheckForActive()) {
            unlockTransition();
            return false;
        }
        atomicStoreMode(OP_MODE_ACTIVE);
        setPausedFlag(false);
        hal_log("[OPMODE] PAUSED → ACTIVE (Sanity-Check bestanden)");
        opModeSavePreference();
        unlockTransition();
        return true;
    }

    if (target == OP_MODE_PAUSED && actual == OP_MODE_ACTIVE) {
        // ACTIVE → PAUSED: Safety LOW und Geschwindigkeit unter Schwellwert
        {
            StateLock lock;
            if (g_nav.safety.safety_ok) {
                hal_log("[OPMODE] ACTIVE→PAUSED verweigert: safety nicht LOW");
                unlockTransition();
                return false;
            }
            const float speed = g_nav.sw.gps_speed_kmh;
            if (speed >= MIN_STEER_SPEED_KMH) {
                hal_log("[OPMODE] ACTIVE→PAUSED verweigert: speed=%.1f km/h >= %.1f",
                        (double)speed, (double)MIN_STEER_SPEED_KMH);
                unlockTransition();
                return false;
            }
        }
        atomicStoreMode(OP_MODE_PAUSED);
        setPausedFlag(true);
        hal_log("[OPMODE] ACTIVE → PAUSED (safety=LOW, speed=%.1f km/h)",
                (double)0.0f);  // speed < threshold, show 0 for clarity
        opModeSavePreference();
        unlockTransition();
        return true;
    }

    // BOOTING → ACTIVE/PAUSED: Nur ueber opModeBootComplete()
    hal_log("[OPMODE] Uebergang %s → %s nicht erlaubt (nutze opModeBootComplete)",
            opModeToString(actual), opModeToString(target));
    unlockTransition();
    return false;
}

void opModeBootComplete(bool safety_low) {
    lockTransition();

    // NVS-Praeferenz laden und beruecksichtigen
    const OpMode nvs_pref = opModeLoadPreference();

    // Wenn safety HIGH, immer ACTIVE (auch wenn NVS PAUSED sagt)
    // Wenn safety LOW, immer PAUSED (auch wenn NVS ACTIVE sagt)
    // Safety-Pin hat Vorrang vor NVS-Praeferenz
    OpMode target;
    if (safety_low) {
        target = OP_MODE_PAUSED;
    } else {
        target = OP_MODE_ACTIVE;
    }

    atomicStoreMode(target);
    setPausedFlag(safety_low);
    hal_log("[OPMODE] BOOTING → %s (safety %s, NVS praeferez=%s)",
            opModeToString(target),
            safety_low ? "LOW" : "HIGH",
            opModeToString(nvs_pref));

    // Praeferenz nur speichern wenn sie vom Safety-Pin bestimmt wurde
    opModeSavePreference();

    unlockTransition();
}

void opModeSafetyChanged(bool safety_ok, float vehicle_speed_kmh) {
    const OpMode current = atomicLoadMode();

    if (current == OP_MODE_ACTIVE && !safety_ok && vehicle_speed_kmh < MIN_STEER_SPEED_KMH) {
        // Automatischer Uebergang ACTIVE → PAUSED wenn Safety-Kick bei Stillstand
        lockTransition();
        // Re-check nach Lock
        if (atomicLoadMode() == OP_MODE_ACTIVE) {
            atomicStoreMode(OP_MODE_PAUSED);
            setPausedFlag(true);
            hal_log("[OPMODE] ACTIVE → PAUSED (safety=LOW, speed=%.1f km/h, auto)",
                    (double)vehicle_speed_kmh);
            opModeSavePreference();
        }
        unlockTransition();
    } else if (current == OP_MODE_PAUSED && safety_ok) {
        // Safety kehrt zurueck — nur loggen, KEIN Auto-Transition (manuell erforderlich)
        hal_log("[OPMODE] Safety wieder OK (speed=%.1f km/h) — manuell aktivieren erforderlich",
                (double)vehicle_speed_kmh);
    }
}

bool opModeIsControlActive(void) {
    return atomicLoadMode() == OP_MODE_ACTIVE;
}

bool opModeIsCommActive(void) {
    return atomicLoadMode() == OP_MODE_ACTIVE;
}

bool opModeIsPaused(void) {
    return atomicLoadMode() == OP_MODE_PAUSED;
}

const char* opModeToString(OpMode mode) {
    switch (mode) {
        case OP_MODE_BOOTING: return "BOOTING";
        case OP_MODE_ACTIVE:  return "ACTIVE";
        case OP_MODE_PAUSED:  return "PAUSED";
        default: return "?";
    }
}

// ===================================================================
// NVS Persistenz (ADR-005)
// ===================================================================

OpMode opModeLoadPreference(void) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(NVS_OP_MODE_NS, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return OP_MODE_ACTIVE;  // Default wenn NVS nicht verfuegbar
    }

    uint8_t value = static_cast<uint8_t>(OP_MODE_ACTIVE);
    err = nvs_get_u8(handle, NVS_OP_MODE_KEY, &value);
    nvs_close(handle);

    if (err != ESP_OK) {
        return OP_MODE_ACTIVE;  // Default wenn Schluessel nicht gefunden
    }

    // Wertebereich pruefen
    if (value > OP_MODE_PAUSED) {
        return OP_MODE_ACTIVE;
    }

    return static_cast<OpMode>(value);
#else
    return OP_MODE_ACTIVE;
#endif
}

void opModeSavePreference(void) {
#if defined(ARDUINO_ARCH_ESP32)
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(NVS_OP_MODE_NS, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        hal_log("[OPMODE] NVS save fehlgeschlagen: %s", esp_err_to_name(err));
        return;
    }

    const uint8_t value = static_cast<uint8_t>(atomicLoadMode());
    err = nvs_set_u8(handle, NVS_OP_MODE_KEY, value);
    if (err != ESP_OK) {
        hal_log("[OPMODE] NVS set fehlgeschlagen: %s", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        hal_log("[OPMODE] NVS commit fehlgeschlagen: %s", esp_err_to_name(err));
    }
#else
    (void)0;  // Kein NVS auf PC-Platform
#endif
}

// ===================================================================
// GPIO Mode Switch (Step 4) — Safety-Pin GPIO 4 Toggle
// ===================================================================
//
// Der Safety-Pin (SAFETY_IN, GPIO 4) wird als Mode-Toggle verwendet:
//   - Pin ist normal HIGH (Safety OK, Pull-Up)
//   - Taste gedrueckt → Pin geht LOW (Safety KICK)
//   - Taste losgelassen → Pin geht HIGH
//
// Die GPIO-Polling erkennt eine FALLING-Flanke (HIGH→LOW) nach
// Debounce (2 aufeinanderfolgende gleiche Readings bei 1s Poll).
// Jede erkannte Flanke toggelt den Modus (ACTIVE↔PAUSED).
//
// Die eigentliche Transition-Logik in opModeRequest() stellt sicher,
// dass:
//   - ACTIVE→PAUSED nur bei safety LOW + speed < Schwellwert
//   - PAUSED→ACTIVE nur bei bestandenem Sanity-Check
//
// Ablauf:
//   1. Normalbetrieb (ACTIVE, Pin HIGH)
//   2. Taste gedrueckt → Pin LOW → automatisch ACTIVE→PAUSED
//      (durch opModeSafetyChanged in control.cpp)
//   3. PAUSED — Wartemodus (Pin HIGH nach Loslassen)
//   4. Taste erneut gedrueckt → Pin LOW → GPIO-Toggle erkennt Flanke
//      → opModeRequest(ACTIVE) → Sanity-Check → ACTIVE
//   5. Taste losgelassen → Pin HIGH → Normalbetrieb
// ===================================================================

void opModeGpioPoll(void) {
    if (!s_gpio_mode_switch_enabled) {
        return;
    }

    const OpMode current = atomicLoadMode();

    // Nur ausserhalb des BOOTING-Modus reagieren
    if (current == OP_MODE_BOOTING) {
        return;
    }

    // Safety-Pin lesen (active LOW: LOW = gedrueckt, HIGH = Normal)
    const bool raw = (digitalRead(SAFETY_IN) == HIGH);  // true = HIGH = OK

    // Debounce: Zaehler fuer aufeinanderfolgende gleiche Readings
    if (raw != s_gpio_last_raw) {
        // Zustand geaendert — Zaehler zuruecksetzen
        s_gpio_last_raw = raw;
        s_gpio_stable_count = 1;
    } else {
        // Zustand stabil — Zaehler erhoehen
        if (s_gpio_stable_count < GPIO_DEBOUNCE_COUNT) {
            s_gpio_stable_count++;
        }
    }

    // Debounce erreicht?
    if (s_gpio_stable_count < GPIO_DEBOUNCE_COUNT) {
        return;
    }

    // Entprellter Zustand hat sich geaendert?
    if (raw == s_gpio_debounced_state) {
        return;  // Kein Zustandswechsel
    }

    // Neuer entprellter Zustand
    s_gpio_debounced_state = raw;

    // FALLING-Flanke erkennen: HIGH → LOW (OK → KICK = Taste gedrueckt)
    if (!raw) {
        // Toggle-Logik: Ziel-Modus ist das Gegenteil vom aktuellen
        OpMode toggle_target = (current == OP_MODE_ACTIVE) ? OP_MODE_PAUSED : OP_MODE_ACTIVE;

        if (toggle_target == OP_MODE_ACTIVE) {
            // PAUSED → ACTIVE: opModeRequest prueft Sanity-Check
            hal_log("[OPMODE] GPIO Toggle: versuche PAUSED → ACTIVE");
        } else {
            // ACTIVE → PAUSED: opModeRequest prueft safety + speed
            hal_log("[OPMODE] GPIO Toggle: versuche ACTIVE → PAUSED");
        }

        const bool accepted = opModeRequest(toggle_target);
        if (!accepted) {
            hal_log("[OPMODE] GPIO Toggle: Uebergang %s → %s abgelehnt",
                    opModeToString(current), opModeToString(toggle_target));
        }
    }
    // RISING-Flanke (LOW → HIGH): Keine Aktion, nur Logging
    else {
        hal_log("[OPMODE] GPIO Safety-Pin wieder HIGH");
    }
}

void opModeSetGpioEnabled(bool enabled) {
    s_gpio_mode_switch_enabled = enabled;
    if (enabled) {
        // Initialen Zustand lesen fuer Edge-Detection
        s_gpio_last_raw = (digitalRead(SAFETY_IN) == HIGH);
        s_gpio_debounced_state = s_gpio_last_raw;
        s_gpio_stable_count = GPIO_DEBOUNCE_COUNT;  // Sofort stabil
    }
    hal_log("[OPMODE] GPIO mode switch %s (Pin=%d)",
            enabled ? "AKTIVIERT" : "DEAKTIVIERT", (int)SAFETY_IN);
}

/// Prueft ob PAUSED-Bit im switchStatus gesetzt werden soll
bool opModeIsPausedStatusBit(void) {
    return atomicLoadMode() == OP_MODE_PAUSED;
}
