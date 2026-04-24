#!/usr/bin/env python3
"""
auto_version.py – PlatformIO pre-build script.

Automatically increments the PATCH version (3rd digit) on every build.
MAJOR.MINOR are read from version.txt (manual control).
PATCH is auto-incremented and written back to version.txt.

When run standalone (not from PlatformIO), the script only prints
the new version and updates version.txt.

When run from PlatformIO (pre:extra_scripts), it also sets
-DFIRMWARE_VERSION as a build flag.

Usage:
    python3 auto_version.py          # standalone: increments version
    # (or via PlatformIO: extra_scripts = pre:auto_version.py)
"""

import os
import sys

# --- Configuration ---
VERSION_FILE = "version.txt"

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    version_path = os.path.join(script_dir, VERSION_FILE)

    # Read current version
    if os.path.exists(version_path):
        with open(version_path, "r") as f:
            content = f.read().strip()
    else:
        content = "0.1.0"

    # Parse version
    parts = content.split(".")
    if len(parts) != 3:
        print(f"[auto_version] ERROR: invalid version '{content}', expected MAJOR.MINOR.PATCH")
        sys.exit(1)

    try:
        major = int(parts[0])
        minor = int(parts[1])
        patch = int(parts[2])
    except ValueError:
        print(f"[auto_version] ERROR: non-numeric version '{content}'")
        sys.exit(1)

    # Auto-increment PATCH
    patch += 1
    new_version = f"{major}.{minor}.{patch}"

    # Write back to version.txt
    with open(version_path, "w") as f:
        f.write(new_version + "\n")

    print(f"[auto_version] {content} -> {new_version}")

    # When called from PlatformIO, Import("env") is available
    # and we inject the version as a build flag.
    try:
        Import("env")
    except NameError:
        # Not running inside PlatformIO – just print and exit
        print(f"[auto_version] (standalone mode – no build flag set)")
        return

    # Running inside PlatformIO – inject build flag
    env = DefaultEnvironment()
    env.Append(BUILD_FLAGS=[f'-DFIRMWARE_VERSION=\\"{new_version}\\"'])

if __name__ == "__main__":
    main()
