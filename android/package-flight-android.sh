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
packages. Transfer this directory into Termux private storage. QEMU's GLib,
zlib and their non-platform shared-library dependencies are bundled here;
Android provides the system C library, math library and dynamic linker. Then
run:

```sh
cd /path/to/REM-FLIGHT-ANDROID-MVP
sha256sum -c SHA256SUMS
chmod 700 scripts/launch-rem.sh
./scripts/launch-rem.sh
```

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
{
  printf 'Kernel: '; sha256sum "$output/kernel/vmlinux"
  printf 'Android QEMU: '; sha256sum "$output/qemu/qemu-system-myemulator32"
  printf 'Rootfs: '; sha256sum "$output/rootfs/rem-rootfs.ext4"
  printf '\nQEMU ELF dependencies:\n'
  readelf -d "$output/qemu/qemu-system-myemulator32" | grep NEEDED || true
} > "$output/docs/ARTIFACTS.txt"
(
  cd "$output"
  find kernel rootfs qemu scripts config docs \( -type f -o -type l \) -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)
printf 'Created Android ARM64 package candidate: %s\n' "$output"
cat "$output/docs/ARTIFACTS.txt"
