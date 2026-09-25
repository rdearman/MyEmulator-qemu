#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 5 ]]; then
  echo "usage: $0 ANDROID_QEMU TERMUX_LIB_DIR VMLINUX ROOTFS_10G OUTPUT_DIR" >&2
  exit 2
fi
qemu=$(realpath "$1")
libs=$(realpath "$2")
kernel=$(realpath "$3")
rootfs=$(realpath "$4")
output=$(realpath -m "$5")
for path in "$qemu" "$kernel" "$rootfs"; do
  [[ -f "$path" && -r "$path" ]] || { echo "missing/unreadable: $path" >&2; exit 2; }
done
[[ -x "$qemu" ]] || { echo "QEMU is not executable: $qemu" >&2; exit 2; }
[[ -d "$libs" ]] || { echo "missing Termux library directory: $libs" >&2; exit 2; }
[[ ! -e "$output" ]] || { echo "refusing to replace existing package: $output" >&2; exit 2; }
qemu_type=$(file -b "$qemu")
[[ "$qemu_type" == *"ELF 64-bit"*"ARM aarch64"* ]] || { echo "not Android ARM64 QEMU: $qemu_type" >&2; exit 2; }
interpreter=$(readelf -l "$qemu" | sed -n 's/.*Requesting program interpreter: \(.*\)]/\1/p')
[[ "$interpreter" == /system/bin/linker64 ]] || { echo "unexpected Android interpreter: $interpreter" >&2; exit 2; }
disk_bytes=$(stat -c %s "$rootfs")
(( disk_bytes >= 10 * 1024 * 1024 * 1024 )) || { echo "guest rootfs is under 10 GiB: $disk_bytes bytes" >&2; exit 2; }
[[ "$(file -b "$rootfs")" == *"ext4 filesystem data"* ]] || { echo "rootfs is not ext4" >&2; exit 2; }

mkdir -p "$output"/{kernel,rootfs,qemu/lib,scripts,config,docs}
install -m 0644 "$kernel" "$output/kernel/vmlinux"
install -m 0755 "$qemu" "$output/qemu/qemu-system-myemulator32"
cp -a "$libs"/. "$output/qemu/lib/"
# Make the shared-library bundle robust to MTP/file-manager transfers, which
# often flatten or drop Unix symlinks. Preserve the SONAME filenames as regular
# files so Android's linker can resolve each DT_NEEDED entry after copying.
while IFS= read -r -d '' link; do
  target=$(readlink -f "$link")
  temporary="${link}.regular-copy"
  cp -p "$target" "$temporary"
  mv -f "$temporary" "$link"
done < <(find "$output/qemu/lib" -type l -print0)
cp --sparse=always --reflink=auto --preserve=mode,timestamps "$rootfs" "$output/rootfs/rem-rootfs.ext4"
install -m 0755 "$(dirname "$0")/launch-rem-flight.sh" "$output/scripts/launch-rem.sh"
cat > "$output/config/rem.conf" <<'EOF'
# Minimum viable REM Android guest configuration.
: "${REM_MACHINE:=myemulator32}"
: "${REM_MEMORY:=128M}"
: "${REM_ICOUNT:=shift=0,sleep=off}"
: "${REM_DISK_FORMAT:=raw}"
: "${REM_SERIAL:=stdio}"
EOF
cat > "$output/docs/README.md" <<'EOF'
# REM phone deployment candidate

This Termux package carries a custom ARM64 QEMU build, matching MyEmulator2
Linux kernel, and persistent ext4 guest disk with 10 GiB capacity. The ext4
filesystem is copied intact from the verified flight userspace and expanded;
BusyBox, the imported GNU userspace, native REM tools, files, permissions,
symlinks, and guest documentation remain in it.

Install Termux from a trusted source and install its `bash` and `coreutils`
packages. Copy the package to shared storage (for example `Download`), then
copy it into Termux's private home before launching; shared Android storage is
commonly mounted `noexec`. In Termux:

```sh
termux-setup-storage
mkdir -p "$HOME/rem"
cp -a "$HOME/storage/downloads/REM-FLIGHT-ANDROID-MVP" "$HOME/rem/"
cd "$HOME/rem/REM-FLIGHT-ANDROID-MVP"
sha256sum -c SHA256SUMS
chmod 700 scripts/launch-rem.sh qemu/qemu-system-myemulator32
chmod 755 qemu/lib/*
./scripts/launch-rem.sh
```

The launcher checks its files, bundled linker dependencies, writable rootfs,
and execution location. Linker errors are printed by Android's dynamic
linker; startup errors and QEMU's exit status remain visible on the terminal.
If Android reports `Permission denied`, move the package under Termux `$HOME`
and check host `adb logcat -d | grep -iE 'avc: denied|qemu'` for SELinux
denials. Do not launch the executable directly from `/sdcard` or `Download`.

This QEMU build also records a build-time `DT_RUNPATH`. The launcher puts the
package's library directory first in `LD_LIBRARY_PATH`; Android's linker
searches that path before `DT_RUNPATH`. `docs/ARTIFACTS.txt` records both the
embedded path and the needed libraries.

The launcher reads `config/rem.conf` and starts machine `myemulator32` with
128 MiB guest RAM, the packaged kernel, a writable 10 GiB ext4 root disk, and
an interactive serial console. It boots from ext4 (`/dev/myemu0`), not an
initramfs. Set `REM_MEMORY=512M` before launching to request more guest RAM.
The guest uses BusyBox ash and vi; `rem-as`, `rem-ld`, and `rem-run` provide
the native development workflow. Persistent writes are periodically synced
and synced when the shell exits. Use `rem-sync` for an explicit flush.

This minimum package intentionally has no guest networking or SSH. The
Android QEMU binary and package are architecture-checked but have not been
boot-tested on a physical Galaxy Fold. The 10 GiB disk requires adequate free
device storage. Keep backups of the writable image.
EOF
(
  cd "$output"
  {
    printf 'Kernel: '; sha256sum kernel/vmlinux
    printf 'Android QEMU: '; sha256sum qemu/qemu-system-myemulator32
    printf 'Rootfs: '; sha256sum rootfs/rem-rootfs.ext4
    printf '\nQEMU ELF dependencies and embedded runtime path:\n'
    readelf -d qemu/qemu-system-myemulator32 | grep -E 'NEEDED|RUNPATH|RPATH' || true
  } > docs/ARTIFACTS.txt
  find kernel rootfs qemu scripts config docs \( -type f -o -type l \) -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)
printf 'Created Android ARM64 package candidate: %s\n' "$output"
cat "$output/docs/ARTIFACTS.txt"
