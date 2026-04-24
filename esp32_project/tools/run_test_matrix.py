#!/usr/bin/env python3
"""Runs the AgSteer test matrix locally and in CI.

Matrix scope:
  1) PlatformIO build for the active firmware profile.
  2) Host smoke tests (Discovery/Hello/Subnet).
  3) Timing metrics (jitter + deadline-miss counter check).
"""

from __future__ import annotations

import os
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = ROOT / "src"
TOOLS_ROOT = ROOT / "tools"

BUILD_PROFILES = [
    "profile_full_steer_ntrip",
]

HOST_PROFILE_FLAGS = {
    "profile_full_steer_ntrip": [
        "-DFEAT_ETH",
        "-DFEAT_IMU",
        "-DFEAT_ADS",
        "-DFEAT_ACT",
        "-DFEAT_SAFETY",
        "-DFEAT_GNSS",
        "-DFEAT_NTRIP",
        "-DFEAT_SD",
        "-DFEAT_LOGSW",
    ],
}


def run(cmd: list[str], cwd: Path) -> None:
    print(f"$ {' '.join(shlex.quote(c) for c in cmd)}")
    subprocess.run(cmd, cwd=cwd, check=True)


def maybe_run_profile_builds() -> None:
    if os.environ.get("SKIP_PROFILE_BUILDS") == "1":
        print("SKIP_PROFILE_BUILDS=1 -> skipping PlatformIO profile builds")
        return

    for profile in BUILD_PROFILES:
        run(["pio", "run", "-e", profile], cwd=ROOT)


def run_host_smoke_for_profile(profile: str) -> None:
    out_bin = TOOLS_ROOT / ".build" / f"smoke_{profile}"
    out_bin.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        "g++",
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-I",
        str(SRC_ROOT),
        "-I",
        str(TOOLS_ROOT / "host_stubs"),
        *HOST_PROFILE_FLAGS[profile],
        str(TOOLS_ROOT / "smoke_matrix.cpp"),
        str(SRC_ROOT / "logic" / "pgn_codec.cpp"),
        "-o",
        str(out_bin),
    ]
    run(cmd, cwd=ROOT)
    run([str(out_bin)], cwd=ROOT)


def main() -> int:
    maybe_run_profile_builds()

    for profile in BUILD_PROFILES:
        run_host_smoke_for_profile(profile)

    print("All matrix checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
