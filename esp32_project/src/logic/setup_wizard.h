/**
 * @file setup_wizard.h
 * @brief First-boot serial setup wizard (Phase 0 / S0-08).
 */

#pragma once

/// Run blocking setup wizard in loop context.
/// Returns true when configuration was saved.
bool setupWizardRun(void);

/// Request wizard execution from CLI/main.
void setupWizardRequestStart(void);

/// Consume pending wizard request flag.
bool setupWizardConsumePending(void);
