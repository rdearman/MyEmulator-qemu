#!/usr/bin/env python3
"""Generate the current MyEmulator2 syscall table from the kernel dispatcher."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "linux/arch/myemulator2/kernel/syscall.c"
OUT = ROOT / "docs/REM_CURRENT_SYSCALL_REFERENCE.md"
text = SRC.read_text()
defines = dict(re.findall(r"#define\s+(MYEMU2_NR_[A-Z0-9_]+)\s+(\d+)", text))
rows = []
for name, number in defines.items():
    if re.search(rf"case\s+{re.escape(name)}\s*:", text):
        rows.append((int(number), name.removeprefix("MYEMU2_NR_").lower(), "dispatcher case"))
for number, name in re.findall(r"case\s+(\d+)\s*:\s*/\*\s*__NR(?:3264)?_([a-zA-Z0-9_]+)", text):
    rows.append((int(number), name.lower(), "dispatcher case"))
rows = sorted(set(rows))
out = ["# Current MyEmulator2 syscall reference", "",
       "Generated from `linux/arch/myemulator2/kernel/syscall.c`. This records "
       "implemented dispatcher cases; it is not a claim of complete Linux ABI "
       "coverage. Tested coverage is a separate fact and is recorded by the "
       "named regression scripts.", "", "The userspace convention is `r1` = "
       "number/result and `r2`–`r7` = arguments. The `syscall` immediate is a "
       "diagnostic field; current Linux handling ignores it, so examples use "
       "`syscall 0`.", "", "| Number | Name | Kernel status |", "|---:|---|---|"]
out += [f"| {n} | `{name}` | {status} |" for n, name, status in rows]
out += ["", "Regenerate with:", "", "```sh", "python3 tools/generate-myemulator2-syscall-reference.py", "```", ""]
OUT.write_text("\n".join(out))
print(f"wrote {OUT} ({len(rows)} implemented cases)")
