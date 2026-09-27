# REM flight image integration

This pipeline builds a disposable candidate from one pinned tuple. It does
not write to `REM-FLIGHT-TTY` or update any source package.

## Inventory findings

`REM-FLIGHT-TTY/scripts/boot-rem-desktop.sh` launches the package's QEMU,
kernel, and 10 GiB rootfs directly; it has no rootfs assembly step.
`toolchain/scripts/build-ext4-rootfs.sh` is a separate small BusyBox rootfs
builder, while `toolchain/scripts/run-linux-ext4.sh` updates the Linux build
configuration, rebuilds the kernel, and boots a supplied image. Neither is
the production flight-package builder.

The TTY package's checksum manifest currently disagrees with its packaged
kernel and rootfs (QEMU and the remaining listed files verify). The documented
September 25 tuple in `.rem-phone-deploy-20260925/REM-FLIGHT` matches the
QEMU/kernel/rootfs hashes in the validated tuple record, so that exact tuple
is the seed selected here. The mismatch was observed read only; the TTY
package was not repaired or changed.

Specifically, the TTY manifest expects kernel `1490a118…ee19d` and rootfs
`74900e51…629fa1`; the current files hash to `e2516e8a…5fa546de` and
`eff36051…d43f12ce` respectively.

## Inputs and tuple

`tuples/flight-20260925.json` selects the desktop QEMU, Linux kernel, and seed
rootfs whose hashes are recorded in `docs/REM_VALIDATED_RUNTIME_TUPLES.md`
and the September 25 package manifest. The binutils qualification tuple uses
`REM-FLIGHT-ANDROID-MVP/rootfs/rem-rootfs.ext4` as its seed source because it
still matches the validated rootfs SHA exactly; the original desktop seed path
changed and now fails its hash check. The `REM-FLIGHT-TTY` package is not
silently substituted. The tuple fixes machine,
RAM, clock mode, guest root device, image size, and required guest files.

The seed includes the already present BusyBox recovery shell, Bash, native
assembler/linker, GNU Make executor, musl static libc, and `rem-sync`. Bash
interactive startup falls back to BusyBox ash; BusyBox provides the `vi`
applet. This tuple selects no C compiler, and has no `libgcc.a`, so it makes
no hosted-C claim. No changing networking, Yash, Samurai, or chibicc output is
added by default. The output manifest records the exact
input paths, sizes, modes, hashes, ELF machine labels, candidate image hash,
and critical guest file hashes/modes/ELF labels.

## Build and verify

Run from the repository root. The output directory must not already exist;
the builder refuses to overwrite it.

```sh
python3 tools/rem-flight-image/build.py \
  --output /tmp/rem-flight-candidate-20260927
python3 tools/rem-flight-image/verify.py \
  /tmp/rem-flight-candidate-20260927
```

The builder first verifies every pinned source hash, extracts the seed into a
disposable normal host tree, and checks required paths. Any selected overlay
is copied with normal host filesystem operations. It recreates `/dev/console`
and `/dev/null` as the required character devices using `fakeroot`, then
creates ext4 with `mkfs.ext4 -d`. It syncs the image, runs read-only
`e2fsck -fn`, and checks required inodes and device numbers in the resulting
image. `debugfs` is used for extraction and read-only inspection only; no
critical binary is written into ext4 with debugfs. There is no mounted image,
so no unmount step is needed. Failures stop the build; verification rejects
missing, empty, truncated, wrong-mode, wrong-hash, wrong-architecture, or
unclean outputs. An incomplete output has no valid manifest and cannot boot
through the provided launcher.

The filesystem contents are rebuilt from pinned inputs and declared policy.
ext4 metadata such as UUID and creation times can differ between builds, so
the full image is verified against the per-build recorded hash rather than
promised to be byte-identical.

## Boot

```sh
/tmp/rem-flight-candidate-20260927/boot.sh
```

The launcher verifies the candidate before running its matching QEMU and
kernel with `myemulator32`, 128 MiB RAM, raw ext4, `-nographic`, and the
validated clock setting. Extra QEMU arguments may be appended to `boot.sh`.
Stop QEMU from its terminal (or with the host timeout used for automated
checks).

## Binutils workstation candidate

The focused native-binutils tuple selects the validated, hash-identical seed
copy plus the accepted tool overlay:

```sh
python3 tools/rem-flight-image/build.py \
  --output /tmp/rem-flight-binutils-final-20260927 \
  --tuple tools/rem-flight-image/tuples/flight-20260927-qualified-binutils.json \
  --artifacts tools/rem-flight-image/overlays/qualified-binutils.json
python3 tools/rem-flight-image/verify.py /tmp/rem-flight-binutils-final-20260927
```

The 2026-09-27 candidate booted on desktop QEMU with a qcow2 copy-on-write
test disk. It reached ash with ext4 and devtmpfs mounted; a minimal REM
executable assembled and linked successfully, and the hash-selected
`readelf`, `objdump`, `nm`, `size`, `strings`, and `strip` commands passed the
focused checks recorded in
[`REM_NATIVE_BINUTILS_QUALIFICATION.md`](../../docs/REM_NATIVE_BINUTILS_QUALIFICATION.md).
Use this as the toolchain qualification candidate; it is not the unified
flight release image and does not include unaccepted compiler, shell, or
Samurai artifacts.

To repeat the interactive smoke without writing to the hash-verified raw
rootfs, create and use a qcow2 overlay:

```sh
qemu-img create -f qcow2 -F raw \
  -b /tmp/rem-flight-binutils-final-20260927/rem-rootfs.ext4 \
  /tmp/rem-flight-binutils-final-20260927/rootfs-overlay.qcow2
/tmp/rem-flight-binutils-final-20260927/qemu-system-myemulator32 \
  -M myemulator32 -m 128M \
  -kernel /tmp/rem-flight-binutils-final-20260927/vmlinux \
  -drive file=/tmp/rem-flight-binutils-final-20260927/rootfs-overlay.qcow2,format=qcow2,if=none,id=myemulator2-disk \
  -nographic -monitor none -serial stdio -icount shift=0,sleep=off
```

## Owner artifact staging and selection

The intake directories are under `staging/`: `applications/`, `binutils/`, `networking/`,
`chibicc-samurai/`, `yash/`, and `offline-kit/`. Place delivered files there
without changing the owners' originals. Staging alone does not accept an
artifact. Copy `artifacts.example.json` to a reviewed overlay manifest, list
each selected file, and record owner, provenance, source, guest destination,
exact size, mode, SHA-256, dependencies, and expected target ELF machine.
Use `dependency_hashes` to pin each required file already in the rootfs or
selected by the overlay; the builder checks and records those hashes. Set
`accepted: true` only after the owner's validation is explicitly accepted.
An overlay may use `remove_guest_files` to remove a known stale regular file;
each removal must pin its current SHA-256, and the final verifier requires it
to be absent.
To exercise a delivered artifact before acceptance, a reviewed overlay may
instead set `accepted: false` and `candidate_test_only: true`. The resulting
manifest labels the image `TEST ONLY`; it must not be selected for flight.
Then build a separate candidate:

```sh
python3 tools/rem-flight-image/build.py \
  --output /tmp/rem-flight-integrated \
  --artifacts /path/to/reviewed-artifacts.json
python3 tools/rem-flight-image/verify.py /tmp/rem-flight-integrated
```

The build fails closed on unaccepted records unless explicitly marked
`candidate_test_only`, unpinned/wrong size or hash, architecture mismatch,
non-executable binaries marked executable, and empty files. Keep the reviewed
overlay JSON with the candidate so selected inputs remain auditable. Nothing
polls owner work or updates the production TTY package.

The current selected-artifact matrix and acceptance handoff are in
[`READINESS.md`](READINESS.md).

## Current boot acceptance

The pinned candidate booted Linux 6.12.1, mounted its root filesystem as
read/write ext4, initialized and mounted devtmpfs, ran `/sbin/init`, and
presented a BusyBox ash prompt. The console accepted an `echo` command and
returned its marker. The image verifier confirmed the existing BusyBox,
Bash, assembler, linker, and `rem-sync` files are nonempty with executable
modes and pinned content. This does not accept Yash, Samurai, or networking.

The guest emitted a sysfs warning during boot. A batch of later guest
utilities also produced faults during smoke interaction, so guest-side
utility execution beyond the confirmed console command is not claimed. The
kernel/QEMU pair remains the published pinned tuple; this integration task
does not change kernel behavior.
