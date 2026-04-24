#!/usr/bin/env python3
"""Validiert Task-Kontext: Branchname und Report-Verzeichnis."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def get_current_branch() -> str:
    try:
        result = subprocess.run(
            ["git", "branch", "--show-current"],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as err:
        stderr = err.stderr.strip() or "unknown git error"
        fail(f"Kann aktuellen Branch nicht ermitteln: {stderr}")

    branch = result.stdout.strip()
    if not branch:
        fail("Aktueller Branch ist leer (detached HEAD?).")
    return branch


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prüft, ob Branch und reports/<Task-ID>/ zum Task passen."
    )
    parser.add_argument(
        "--task-id",
        required=True,
        help="Task-ID, z. B. TASK-1234",
    )
    args = parser.parse_args()

    task_id = args.task_id.strip()
    if not task_id:
        fail("Task-ID darf nicht leer sein.")

    expected_branch = f"task/{task_id}"
    current_branch = get_current_branch()
    if current_branch != expected_branch:
        fail(
            "Falscher Branch für Task-Arbeit: "
            f"aktuell '{current_branch}', erwartet '{expected_branch}'. "
            "Sofort stoppen und eskalieren."
        )

    report_dir = ROOT / "reports" / task_id
    if not report_dir.is_dir():
        fail(
            f"Fehlender Report-Ordner: '{report_dir}'. "
            "Lege reports/<Task-ID>/ an oder eskaliere an Planer/Verantwortliche."
        )

    print(
        "OK: Task-Kontext gültig "
        f"(branch='{current_branch}', report_dir='{report_dir.relative_to(ROOT)}')."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
