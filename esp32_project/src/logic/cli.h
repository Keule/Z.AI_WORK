/**
 * @file cli.h
 * @brief Serial Command Line Interface — Phase 0 (S0-01).
 *
 * Provides a lightweight command shell for runtime configuration,
 * diagnostics, and testing via the Serial console.
 */

#pragma once

#include <cstddef>
#include <Stream.h>

/// Initialisiert das CLI-System. Einmalig aus setup() aufrufen.
void cliInit(void);

/// Verarbeitet eine komplette Eingabezeile.
/// Wird aus task_slow aufgerufen wenn eine Zeile komplett empfangen wurde.
void cliProcessLine(const char* line);

/// Setzt das Ausgabemedium (Standard: Serial).
void cliSetOutput(Stream* out);

/// Registriert ein neues Kommando.
/// Gibt true zurück bei Erfolg, false bei Duplikat oder voller Tabelle.
bool cliRegisterCommand(const char* cmd,
                        void (*handler)(int argc, char* argv[]),
                        const char* help_short);

/// Gibt alle registrierten Kommandos auf Serial aus.
void cliPrintHelp(void);
