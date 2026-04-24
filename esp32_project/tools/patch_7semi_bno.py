from pathlib import Path

Import("env")


def patch_7semi_bno(source=None, target=None, env=None):
    pioenv = env.subst("$PIOENV")
    lib_file = (
        Path(env.subst("$PROJECT_DIR"))
        / ".pio"
        / "libdeps"
        / pioenv
        / "7Semi BNO08x"
        / "src"
        / "BnoSPIBus.h"
    )

    if not lib_file.exists():
        print(f"7Semi BNO patch skipped: {lib_file} not found yet")
        return

    original = lib_file.read_text(encoding="utf-8")
    patched = original.replace("    delay(3);\n", "    delayMicroseconds(1000);\n")

    if patched != original:
        lib_file.write_text(patched, encoding="utf-8")
        print("7Semi BNO patch applied: rx post-read delay 3ms -> 1000us")


patch_7semi_bno(env=env)
