#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/qemu-checkout" >&2
    exit 2
fi

qemu_tree="$1"
overlay_root="$(cd "$(dirname "$0")/.." && pwd)"

if [ ! -x "$qemu_tree/configure" ]; then
    echo "not a full QEMU checkout: $qemu_tree" >&2
    exit 1
fi

mkdir -p "$qemu_tree/target/myemulator"
mkdir -p "$qemu_tree/hw/myemulator"
mkdir -p "$qemu_tree/configs/targets"
mkdir -p "$qemu_tree/configs/devices/myemulator-softmmu"
mkdir -p "$qemu_tree/gdb-xml"

cp "$overlay_root/target/myemulator/"* "$qemu_tree/target/myemulator/"
cp "$overlay_root/hw/myemulator/"* "$qemu_tree/hw/myemulator/"
cp "$overlay_root/configs/targets/myemulator-softmmu.mak" "$qemu_tree/configs/targets/"
cp "$overlay_root/configs/devices/myemulator-softmmu/default.mak" \
    "$qemu_tree/configs/devices/myemulator-softmmu/"
cp "$overlay_root/gdb-xml/myemulator-core.xml" "$qemu_tree/gdb-xml/"

grep -qxF "subdir('myemulator')" "$qemu_tree/target/meson.build" || \
    printf "subdir('myemulator')\n" >> "$qemu_tree/target/meson.build"

grep -qxF "source myemulator/Kconfig" "$qemu_tree/target/Kconfig" || \
    printf "source myemulator/Kconfig\n" >> "$qemu_tree/target/Kconfig"

grep -qxF "subdir('myemulator')" "$qemu_tree/hw/meson.build" || \
    printf "subdir('myemulator')\n" >> "$qemu_tree/hw/meson.build"

grep -qxF "source myemulator/Kconfig" "$qemu_tree/hw/Kconfig" || \
    printf "source myemulator/Kconfig\n" >> "$qemu_tree/hw/Kconfig"

python3 - "$qemu_tree/include/sysemu/arch_init.h" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()
needle = "    QEMU_ARCH_LOONGARCH = (1 << 23),"
replacement = needle + "\n    QEMU_ARCH_MYEMULATOR = (1 << 24),"
if "QEMU_ARCH_MYEMULATOR" not in text:
    if needle not in text:
        raise SystemExit("could not find QEMU architecture enum insertion point")
    path.write_text(text.replace(needle, replacement))
PY

python3 - "$qemu_tree/qapi/machine.json" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()
needle = "'mipsel', 'or1k'"
replacement = "'mipsel', 'myemulator', 'or1k'"
if replacement not in text:
    text = text.replace(needle, replacement)
    path.write_text(text)
PY

python3 - "$qemu_tree/meson.build" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()
if "    'hw/myemulator'," not in text:
    needle = "    'hw/gpio',"
    if needle not in text:
        raise SystemExit("could not find trace-events insertion point")
    path.write_text(text.replace(needle, needle + "\n    'hw/myemulator',", 1))
PY

# Native MyEmulator stepping uses QEMU's TCG debug-stop path without opening
# the GDB remote server.  Upstream assumes every debug stop has a GDB process;
# make that assumption explicit so a non-GDB debugger can use the same path.
python3 - "$qemu_tree/include/exec/gdbstub.h" "$qemu_tree/gdbstub/gdbstub.c" "$qemu_tree/system/cpus.c" <<'PY'
import pathlib
import sys

header, stub, cpus = map(pathlib.Path, sys.argv[1:])
h = header.read_text()
needle = "void gdb_set_stop_cpu(CPUState *cpu);"
replacement = needle + "\nbool gdbstub_has_client(void);"
if "bool gdbstub_has_client(void);" not in h:
    if needle not in h:
        raise SystemExit("could not patch gdbstub header")
    header.write_text(h.replace(needle, replacement, 1))

s = stub.read_text()
needle = "void gdb_set_stop_cpu(CPUState *cpu)\n{"
replacement = "bool gdbstub_has_client(void)\n{\n    return gdbserver_state.process_num > 0;\n}\n\n" + needle
if "bool gdbstub_has_client(void)" not in s:
    if needle not in s:
        raise SystemExit("could not patch gdbstub implementation")
    stub.write_text(s.replace(needle, replacement, 1))

c = cpus.read_text()
needle = "        gdb_set_stop_cpu(cpu);\n        qemu_system_debug_request();"
replacement = "        if (gdbstub_has_client()) {\n            gdb_set_stop_cpu(cpu);\n        }\n        qemu_system_debug_request();"
if replacement not in c:
    if needle not in c:
        raise SystemExit("could not patch CPU debug-stop path")
    cpus.write_text(c.replace(needle, replacement, 1))
PY
