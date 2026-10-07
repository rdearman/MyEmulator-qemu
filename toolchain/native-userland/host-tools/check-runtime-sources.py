import json
from pathlib import Path
import os
import subprocess

root = Path(os.environ.get("REM_NATIVE_WORK", "/tmp/rem-native-userland-work"))
source = root / "kit/bootstrap/musl"
output = root / "bootstrap-host-syntax/runtime"
output.mkdir(exist_ok=True)
compiler = root / "bootstrap-host-syntax/chibicc-host-staged"
findings = json.loads((root / "large-frame-audit.json").read_text())["libc"]
members = list(dict.fromkeys(item["member"][:-2] for item in findings))
members.append("strtod")
results = []
for member in members:
    paths = list((source / "src").rglob(member + ".c"))
    if len(paths) != 1:
        raise RuntimeError(f"ambiguous source for {member}: {paths}")
    command = [str(compiler), "-S", "-D_XOPEN_SOURCE=700"]
    command += ["-I" + path for path in (
        "arch/myemulator2", "arch/generic", "src/include", "src/internal",
        "native-headers", "include")]
    command += [str(paths[0].relative_to(source)), "-o", str(output / (member + ".s"))]
    result = subprocess.run(command, cwd=source, capture_output=True, text=True, timeout=60)
    (output / (member + ".log")).write_text(result.stdout + result.stderr)
    results.append({"member": member, "exit": result.returncode})
    print(member, result.returncode, result.stderr[-400:].replace("\n", " "), flush=True)
(output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
raise SystemExit(any(item["exit"] for item in results))
