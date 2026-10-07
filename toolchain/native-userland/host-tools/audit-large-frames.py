import json
import os
import re
from pathlib import Path

root = Path(os.environ.get("REM_NATIVE_WORK", "/tmp/rem-native-userland-work"))
report = {}
for library in ("libc", "libgcc"):
    member = function = None
    saved = False
    incoming = False
    clobber = None
    findings = []
    for line in (root / f"{library}-baseline.disassembly").read_text().splitlines():
        match = re.match(r"(\S+\.o):\s+file format", line)
        if match:
            member = match[1]
        match = re.match(r"[0-9a-f]+ <([^>]+)>:", line)
        if match:
            function = match[1]
            incoming = saved = False
            clobber = None
            continue
        if function is None or saved:
            continue
        if re.search(r"\badd r12,r15,r0\b", line):
            incoming = True
        elif re.search(r"\b(?:lui|ori|li|addi|subi|lw) r12,", line):
            clobber = line.strip()
        elif re.search(r"\bsw r12,4\(r13\)", line):
            if clobber:
                findings.append(
                    {"member": member, "function": function, "clobber": clobber}
                )
            saved = True
    report[library] = findings
    print(library, len(findings), "affected functions")
    for finding in findings:
        print(f"  {finding['member']}: {finding['function']}")
(root / "large-frame-audit.json").write_text(json.dumps(report, indent=2) + "\n")
