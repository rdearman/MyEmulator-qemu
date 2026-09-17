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
