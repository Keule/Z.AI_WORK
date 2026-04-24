/**
 * @file diag.cpp
 * @brief Lightweight diagnostics helpers for serial CLI (Phase 0 / S0-07).
 */

#include "diag.h"

#include "module_interface.h"
#include "runtime_config.h"
#include "hal/hal.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void diagPrintHw(void) {
    Serial.println("Diag HW:");
    Serial.printf("  ETH:    %s\n", moduleSysIsActive(ModuleId::ETH) && hal_net_detected() ? "OK" : "FAIL");
    const auto* imu_mod = moduleSysGet(ModuleId::IMU);
    Serial.printf("  IMU:    %s\n", imu_mod && imu_mod->state.detected ? "OK" : "FAIL");
    const auto* was_mod = moduleSysGet(ModuleId::WAS);
    Serial.printf("  ADS:    %s\n", was_mod && was_mod->state.detected ? "OK" : "FAIL");
    const auto* act_mod = moduleSysGet(ModuleId::ACTUATOR);
    Serial.printf("  ACT:    %s\n", act_mod && act_mod->state.detected ? "OK" : "FAIL");
    Serial.printf("  SAFETY: %s\n", hal_safety_ok() ? "OK" : "KICK");
    const auto* log_mod = moduleSysGet(ModuleId::LOGGING);
    Serial.printf("  SD:     %s\n", log_mod && log_mod->state.detected ? "PRESENT" : "MISSING");
}

void diagPrintMem(void) {
    Serial.println("Diag MEM:");
    Serial.printf("  Heap free:    %lu KB\n", static_cast<unsigned long>(ESP.getFreeHeap() / 1024UL));
    Serial.printf("  Heap largest: %lu KB\n", static_cast<unsigned long>(ESP.getMaxAllocHeap() / 1024UL));
    Serial.printf("  PSRAM free:   %lu KB\n", static_cast<unsigned long>(ESP.getFreePsram() / 1024UL));
    Serial.printf("  Tasks:        %lu\n", static_cast<unsigned long>(uxTaskGetNumberOfTasks()));
}

void diagPrintNet(void) {
    RuntimeConfig& cfg = softConfigGet();
    Serial.println("Diag NET:");
    Serial.printf("  Mode: %s\n", cfg.net_mode == 0 ? "DHCP" : "STATIC");

    const uint32_t ip = hal_net_get_ip();
    const uint32_t gw = hal_net_get_gateway();
    const uint32_t mask = hal_net_get_subnet();

    Serial.printf("  IP:    %u.%u.%u.%u\n",
                  static_cast<unsigned>((ip >> 24) & 0xFF),
                  static_cast<unsigned>((ip >> 16) & 0xFF),
                  static_cast<unsigned>((ip >> 8) & 0xFF),
                  static_cast<unsigned>(ip & 0xFF));
    Serial.printf("  GW:    %u.%u.%u.%u\n",
                  static_cast<unsigned>((gw >> 24) & 0xFF),
                  static_cast<unsigned>((gw >> 16) & 0xFF),
                  static_cast<unsigned>((gw >> 8) & 0xFF),
                  static_cast<unsigned>(gw & 0xFF));
    Serial.printf("  MASK:  %u.%u.%u.%u\n",
                  static_cast<unsigned>((mask >> 24) & 0xFF),
                  static_cast<unsigned>((mask >> 16) & 0xFF),
                  static_cast<unsigned>((mask >> 8) & 0xFF),
                  static_cast<unsigned>(mask & 0xFF));
    Serial.printf("  Link:  %s\n", hal_net_link_up() ? "UP" : "DOWN");
    Serial.printf("  Conn:  %s\n", hal_net_is_connected() ? "UP" : "DOWN");
}
