#!/usr/bin/env python3
"""Exercise REM GCC callee-saved R5 preservation across a call."""

from __future__ import annotations

import importlib.util
import re
import subprocess
import tempfile
from pathlib import Path


SOURCE = r'''
__attribute__((noinline)) int helper(int x) { return x + 1; }
__attribute__((noinline)) int probe(int x)
{
    register int keep asm("r5") = x;
    return helper(keep) + keep;
}
int main(void) { return probe(20) == 41 ? 0 : 1; }
'''


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    gcc = root / ".toolchain-install/bin/myemulator2-elf-gcc"
    if not gcc.is_file():
        raise SystemExit(f"missing compiler: {gcc}")
    with tempfile.TemporaryDirectory(prefix="rem-gcc-callee-") as td:
        out = Path(td)
        source = out / "callee.c"
        assembly = out / "callee.s"
        crt_obj = out / "crt0.o"
        object_file = out / "callee.o"
        elf = out / "callee.elf"
        source.write_text(SOURCE)
        subprocess.run([str(gcc), "-O0", "-S", str(source), "-o", str(assembly)], check=True)
        function = assembly.read_text().split("probe:", 1)[1].split(".size", 1)[0]
        if "sw r5, 0(sp)" not in function or "lw r5, 0(r12)" not in function:
            raise SystemExit("callee-saved R5 save/restore sequence missing")
        common = ["-ffreestanding", "-fno-builtin", "-fno-asynchronous-unwind-tables",
                  "-nostdlib", "-nostartfiles", "-nodefaultlibs"]
        subprocess.run([str(gcc), *common, "-c", str(root / "toolchain/examples/crt0.S"),
                        "-o", str(crt_obj)], check=True)
        subprocess.run([str(gcc), *common, "-O0", "-c", str(source), "-o", str(object_file)],
                       check=True)
        subprocess.run([str(gcc), *common, str(crt_obj), str(object_file), "-lgcc",
                        "-o", str(elf)], check=True)
        helper_path = root / "toolchain/tests/test-binutils.py"
        spec = importlib.util.spec_from_file_location("rem_binutils_test", helper_path)
        helper = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(helper)
        helper.run_elf(str(root / ".qemu-build/qemu-system-myemulator32"), elf, 0, "R1")
    print("REM GCC callee-saved R5 regression: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
