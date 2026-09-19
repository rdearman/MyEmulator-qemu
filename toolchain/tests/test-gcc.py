#!/usr/bin/env python3
"""Freestanding GCC -> GAS -> LD -> MyEmulator2 execution tests."""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def run(command, *, cwd=None, env=None):
    result = subprocess.run(command, cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise AssertionError(f"{command!r} failed:\n{result.stdout}\n{result.stderr}")
    return result


def qemu_result(qemu, elf, register="R1"):
    # Reuse the proven QMP execution helper from the binutils suite without
    # making the GCC tests depend on an installed Python package.
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "test_binutils", Path(__file__).with_name("test-binutils.py"))
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    # The helper checks for a halted CPU and the expected register value.
    module.run_elf(qemu, elf, 0, register)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: test-gcc.py GCC-BIN ROOT")
    gcc = Path(sys.argv[1]).resolve()
    root = Path(sys.argv[2]).resolve()
    qemu = os.environ.get("QEMU_MYEMULATOR32",
                          str(root / ".qemu-build/qemu-system-myemulator32"))
    gcc_bin = gcc.parent
    if not gcc.exists():
        raise AssertionError(f"missing compiler: {gcc}")

    env = os.environ.copy()
    env["PATH"] = str(gcc_bin) + os.pathsep + env.get("PATH", "")
    include = ["-ffreestanding", "-fno-builtin", "-nostdlib",
               "-nostartfiles", "-nodefaultlibs"]
    crt = root / "toolchain/examples/crt0.S"
    sources = [root / "toolchain/tests/c-suite.c",
               root / "toolchain/tests/c-helper.c",
               root / "toolchain/tests/c-data.c"]
    frame_source = root / "toolchain/tests/c-call-frames.c"
    levels = ("-O0", "-O1", "-O2", "-Os")
    with tempfile.TemporaryDirectory(prefix="myemu2-gcc-") as name:
        out = Path(name)
        for level in levels:
            objects = []
            crt_o = out / f"crt0-{level[1:]}.o"
            run([str(gcc), *include, "-c", str(crt), "-o", str(crt_o)],
                env=env)
            objects.append(crt_o)
            for source in sources:
                obj = out / f"{source.stem}-{level[1:]}.o"
                run([str(gcc), *include, level, "-c", str(source), "-o",
                     str(obj)], env=env)
                objects.append(obj)
            elf = out / f"c-suite-{level[1:]}.elf"
            run([str(gcc), *include, "-Wl,--gc-sections", *map(str, objects),
                 "-lgcc",
                 "-o", str(elf)], env=env)
            header = run([str(gcc_bin / "myemulator2-elf-readelf"), "-h",
                          str(elf)], env=env).stdout
            assert "ELF32" in header and "MyEmulator2" in header, header
            listing = run([str(gcc_bin / "myemulator2-elf-objdump"), "-d",
                           str(elf)], env=env).stdout
            assert re.search(r"\b(call|jal)\b", listing, re.I), listing
            if Path(qemu).exists():
                # The crt0 leaves main's return value in R1 and HALT preserves it.
                module = None
                import importlib.util
                spec = importlib.util.spec_from_file_location(
                    "test_binutils", Path(__file__).with_name("test-binutils.py"))
                module = importlib.util.module_from_spec(spec)
                assert spec.loader is not None
                spec.loader.exec_module(module)
                module.run_elf(qemu, elf, 0, "R1")

            frame_obj = out / f"c-call-frames-{level[1:]}.o"
            frame_elf = out / f"c-call-frames-{level[1:]}.elf"
            run([str(gcc), *include, level, "-c", str(frame_source), "-o",
                 str(frame_obj)], env=env)
            run([str(gcc), *include, str(crt_o), str(frame_obj), "-lgcc",
                 "-o", str(frame_elf)], env=env)
            if Path(qemu).exists():
                module.run_elf(qemu, frame_elf, 0, "R1")
        print(f"GCC C suite: PASS ({len(levels)} optimization levels)")


if __name__ == "__main__":
    main()
