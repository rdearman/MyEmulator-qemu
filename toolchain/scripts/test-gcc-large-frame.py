#!/usr/bin/env python3
"""Regression test for REM GCC large-frame link-register preservation.

The old backend materialised a large frame size in R12 after copying the
incoming frame pointer there, then saved the frame-size constant as the
caller's frame pointer.  This test checks the generated prologue directly;
it does not require native GCC or a guest image.
"""

from __future__ import annotations

import argparse
import importlib.util
import re
import subprocess
import tempfile
from pathlib import Path


SOURCE = r"""
volatile int sink;
__attribute__((noinline)) int frame_helper(int x) { return x + 1; }
__attribute__((noinline)) int large_frame(int x)
{
    volatile char frame[70000];
    frame[0] = (char)x;
    frame[69999] = (char)(x + 1);
    return frame[0] + frame[69999] + sink;
}
__attribute__((noinline)) int medium_frame(int x)
{
    volatile char frame[1152];
    frame[0] = (char)x;
    frame[1151] = (char)(x + 1);
    return frame_helper(frame[0]) + frame[1151] + sink;
}
int main(void)
{
    return large_frame(7) == 15 && medium_frame(7) == 16 ? 0 : 1;
}
"""


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--gcc",
        type=Path,
        default=root / ".toolchain-install/bin/myemulator2-elf-gcc",
    )
    args = parser.parse_args()
    if not args.gcc.is_file():
        raise SystemExit(f"missing compiler: {args.gcc}")

    with tempfile.TemporaryDirectory(prefix="rem-gcc-frame-") as td:
        td_path = Path(td)
        source = td_path / "large-frame.c"
        assembly = td_path / "large-frame.s"
        crt = root / "toolchain/examples/crt0.S"
        crt_obj = td_path / "crt0.o"
        frame_obj = td_path / "large-frame.o"
        elf = td_path / "large-frame.elf"
        source.write_text(SOURCE)
        subprocess.run(
            [str(args.gcc), "-O0", "-S", str(source), "-o", str(assembly)],
            check=True,
        )
        text = assembly.read_text()
        function = text.split("large_frame:", 1)[1].split(".size", 1)[0]

        if "add r12, r15, r0" not in function:
            raise SystemExit("large-frame prologue does not preserve incoming R15")
        if len(re.findall(r"subi sp, sp, 255", function)) < 2:
            raise SystemExit("large-frame prologue did not use immediate stack decrements")
        if re.search(r"lui r12, 0x1\s*\n\s*ori r12, r12,", function):
            raise SystemExit("large-frame prologue reuses R12 for frame size")
        if "sw r12," not in function:
            raise SystemExit("large-frame prologue does not save the incoming frame pointer")
        # The epilogue must restore the caller frame through the saved
        # caller-frame slot.  Loading the frame-size temporary directly into
        # r15 is the historical defect that caused native binutils to lose
        # their frame pointer and later fault at low addresses.
        if "lw r11, 4(r12)" not in function or "add r15, r11, r0" not in function:
            raise SystemExit("large-frame epilogue does not restore the caller frame pointer")

        medium = text.split("medium_frame:", 1)[1].split(".size", 1)[0]
        if "add r12, r15, r0" not in medium:
            raise SystemExit("medium-frame prologue does not preserve incoming R15")
        if "sw r12, 52(sp)" not in medium:
            raise SystemExit("medium-frame prologue does not save the incoming frame pointer")
        if "addi r12, r0, 1152" in medium:
            raise SystemExit("medium-frame prologue overwrites the saved frame pointer")

        common = ["-ffreestanding", "-fno-builtin", "-fno-asynchronous-unwind-tables",
                  "-nostdlib", "-nostartfiles", "-nodefaultlibs"]
        subprocess.run([str(args.gcc), *common, "-c", str(crt), "-o", str(crt_obj)],
                       check=True)
        subprocess.run([str(args.gcc), *common, "-O0", "-c", str(source),
                        "-o", str(frame_obj)], check=True)
        subprocess.run([str(args.gcc), *common, str(crt_obj), str(frame_obj), "-lgcc",
                        "-o", str(elf)], check=True)
        helper_path = root / "toolchain/tests/test-binutils.py"
        spec = importlib.util.spec_from_file_location("rem_binutils_test", helper_path)
        helper = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(helper)
        helper.run_elf(str(root / ".qemu-build/qemu-system-myemulator32"), elf, 0, "R1")
        print("REM GCC large-frame frame-pointer regression: PASS")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
