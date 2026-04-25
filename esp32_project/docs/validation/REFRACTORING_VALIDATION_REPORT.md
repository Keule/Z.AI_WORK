# ZAI_GPS Refactoring — Validierungs-Report

Datum: 2026-04-22
Branch: refactoring/phase4-controlstep-unbundle
Commit: TBD

## Compile-Matrix
| Profil | Result | Errors | Warnings | Hinweis |
|--------|--------|--------|----------|---------|
| full_steer | N/A | N/A | N/A | `pio` in dieser Umgebung nicht installiert |
| sensor_front | N/A | N/A | N/A | `pio` in dieser Umgebung nicht installiert |
| actor_rear | N/A | N/A | N/A | `pio` in dieser Umgebung nicht installiert |
| comm_only | N/A | N/A | N/A | `pio` in dieser Umgebung nicht installiert |

## Unit-Tests
| Test-Suite | Tests | Passed | Failed | Hinweis |
|-----------|-------|--------|--------|---------|
| test_pgn_codec | 3 | N/A | N/A | `pio test -e native` nicht ausfuehrbar |
| test_pid | 4 | N/A | N/A | `pio test -e native` nicht ausfuehrbar |
| test_dependency_policy | 3 | N/A | N/A | `pio test -e native` nicht ausfuehrbar |
| test_state_structs | 3 | N/A | N/A | `pio test -e native` nicht ausfuehrbar |
| test_control_phases | 4 | N/A | N/A | `pio test -e native` nicht ausfuehrbar |

## Binary-Size
| Profil | Flash (vorher) | Flash (nachher) | RAM (vorher) | RAM (nachher) |
|--------|---------------|----------------|-------------|--------------|
| full_steer | N/A | N/A | N/A | N/A |
| comm_only | N/A | N/A | N/A | N/A |

## Statische Validierung (ausgefuehrt)
| Check | Result |
|------|--------|
| `rg "g_nav\.(heading_deg|roll_deg|...|timestamp_ms)" src` | PASS (keine alten direkten Felder) |
| `rg "steerAngle|steer_angle\." src/logic --type cpp` | PASS (nur Legacy-Kompatibilitaet) |
| `rg "struct SensorSnapshot|controlReadSafety|controlWriteState" src/logic/control.*` | PASS |

## Hardware-Tests
| Test | Result |
|------|--------|
| Boot-Sequence | N/A (keine Hardware im Container) |
| Safety-KICK | N/A |
| IMU-Data-Flow | N/A |
| WAS-Data-Flow | N/A |
| PGN-AgIO-Kommunikation | N/A |
| Auto-Steer | N/A |
| Watchdog-Timeout | N/A |
| SD-Logger | N/A |

## Offene Punkte
- Vollstaendige Ausfuehrung von Compile-Matrix, Unit-Tests und HIL-Checks auf einem System mit installiertem PlatformIO und angeschlossener Hardware nachholen.
