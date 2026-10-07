"""Host-only REM assembly checks; these are not native execution acceptance."""
import os
import json
from pathlib import Path
import subprocess

root = Path(os.environ.get("REM_NATIVE_WORK", "/tmp/rem-native-userland-work"))
source = root / "kit/bootstrap/compiler-rt-15.0.7.src"
headers = root / "kit/bootstrap/musl"
output = root / "bootstrap-host-syntax/ieee-integer"
output.mkdir(exist_ok=True)
compiler = root / "bootstrap-host-syntax/chibicc-host-ieee-integer"
files = (
    "addsf3 adddf3 subsf3 subdf3 mulsf3 muldf3 divsf3 divdf3 "
    "comparesf2 comparedf2 extendsfdf2 truncdfsf2 "
    "fixsfsi fixdfsi fixunssfsi fixunsdfsi fixsfdi fixdfdi fixunssfdi fixunsdfdi "
    "floatsisf floatsidf floatdisf floatdidf floatunsisf floatunsidf "
    "floatundisf floatundidf fp_mode"
).split()
results = []
for name in files:
    command = [str(compiler), "-S", "-D__SOFTFP__=1"]
    command += ["-I" + str(headers / path) for path in (
        "arch/myemulator2", "arch/generic", "src/include", "src/internal",
        "native-headers", "include")]
    command += ["lib/builtins/" + name + ".c", "-o", str(output / (name + ".s"))]
    result = subprocess.run(command, cwd=source, capture_output=True,
                            text=True, timeout=60)
    (output / (name + ".log")).write_text(result.stdout + result.stderr)
    results.append({"source": name, "exit": result.returncode})
    print(name, result.returncode, result.stderr[-500:].replace("\n", " "),
          flush=True)
(output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
raise SystemExit(any(item["exit"] for item in results))
