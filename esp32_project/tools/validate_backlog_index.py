#!/usr/bin/env python3
"""Kleiner Validator für backlog/index.yaml."""

from __future__ import annotations

from pathlib import Path
import sys

try:
    import yaml
except ModuleNotFoundError:
    print("ERROR: Missing dependency 'PyYAML'. Install with: pip install pyyaml", file=sys.stderr)
    sys.exit(2)

ROOT = Path(__file__).resolve().parents[1]
INDEX_FILE = ROOT / "backlog" / "index.yaml"


def fail(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    if not INDEX_FILE.exists():
        fail(f"{INDEX_FILE} not found")

    data = yaml.safe_load(INDEX_FILE.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        fail("index root must be a mapping")

    enums = data.get("enums", {})
    tasks = data.get("tasks", [])
    epics = data.get("epics", [])

    required_enums = ("status", "priority", "delivery_mode", "task_category")
    for enum_name in required_enums:
        values = enums.get(enum_name)
        if not isinstance(values, list) or not values:
            fail(f"enums.{enum_name} must be a non-empty list")

    task_ids = set()
    for task in tasks:
        task_id = task.get("id")
        if not task_id:
            fail("task without id")
        if task_id in task_ids:
            fail(f"duplicate task id: {task_id}")
        task_ids.add(task_id)

        for field, enum_name in (
            ("status", "status"),
            ("priority", "priority"),
            ("delivery_mode", "delivery_mode"),
            ("task_category", "task_category"),
        ):
            value = task.get(field)
            if value not in enums[enum_name]:
                fail(f"task {task_id}: invalid {field} '{value}'")

        rel_file = task.get("file")
        if not rel_file or not (ROOT / rel_file).exists():
            fail(f"task {task_id}: file not found '{rel_file}'")

        deps = task.get("dependencies", [])
        if not isinstance(deps, list):
            fail(f"task {task_id}: dependencies must be a list")

    epic_ids = set()
    for epic in epics:
        epic_id = epic.get("id")
        if not epic_id:
            fail("epic without id")
        if epic_id in epic_ids:
            fail(f"duplicate epic id: {epic_id}")
        epic_ids.add(epic_id)

        rel_file = epic.get("file")
        if not rel_file or not (ROOT / rel_file).exists():
            fail(f"epic {epic_id}: file not found '{rel_file}'")

        task_refs = epic.get("task_ids", [])
        if not isinstance(task_refs, list):
            fail(f"epic {epic_id}: task_ids must be a list")
        missing = [tid for tid in task_refs if tid not in task_ids]
        if missing:
            fail(f"epic {epic_id}: unknown task refs: {', '.join(missing)}")

    for task in tasks:
        task_id = task["id"]
        for dep in task.get("dependencies", []):
            if dep not in task_ids:
                fail(f"task {task_id}: unknown dependency '{dep}'")

        epic_id = task.get("epic_id")
        if epic_id not in epic_ids:
            fail(f"task {task_id}: unknown epic_id '{epic_id}'")

    print("OK: backlog/index.yaml is valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
