/**
 * @file diag.h
 * @brief Lightweight diagnostics helpers for serial CLI (Phase 0 / S0-07).
 */

#pragma once

/// Print hardware/module detection summary.
void diagPrintHw(void);

/// Print memory/runtime summary.
void diagPrintMem(void);

/// Print network summary.
void diagPrintNet(void);
