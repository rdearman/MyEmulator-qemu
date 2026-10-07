#!/usr/bin/env python3
"""Host-only required-source compile check; never substitutes for native acceptance."""
import os
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(os.environ.get("REM_NATIVE_WORK", "/tmp/rem-native-userland-work"))
tools = Path(__file__).resolve().parent
package = root / "kit/packages" / sys.argv[1]
plan = package / "rem-native-plan"
output = root / "evidence" / (package.name + "-required-source-check")
output.mkdir(exist_ok=True)
jobs = []
lines = (plan / "build.ninja.in").read_text().splitlines()
for i, line in enumerate(lines):
    if ": cc " not in line:
        continue
    out, source = line[6:].split(": cc ", 1)
    source = source.split(" | ", 1)[0]
    flags = shlex.split(lines[i + 2].split(" = ", 1)[1].replace("$$", "$")
                       .replace("@REM_DEPS@", str(root / "host-build-plans/native-deps")))
    jobs.append((out, source, flags))
results = []
for number, (out, source, flags) in enumerate(jobs):
    target = output / (out.replace("/", "-") + ".s")
    command = [str(tools / "chibicc-configure-cc"), "-S", *flags,
               source, "-o", str(target)]
    result = subprocess.run(command, cwd=plan, capture_output=True,
                            text=True, timeout=120)
    log = output / (out.replace("/", "-") + ".log")
    log.write_text(result.stdout + result.stderr)
    results.append({"source": source, "exit": result.returncode})
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    if result.returncode:
        print(f"FAILED {number + 1}/{len(jobs)} {source}\n{result.stderr[-2500:]}",
              flush=True)
        raise SystemExit(1)
    if number % 50 == 0:
        print(f"EMITTED {number + 1}/{len(jobs)} {source}", flush=True)
print(f"REM_SOURCE_EMISSION_OK {len(jobs)}; native execution still required")
