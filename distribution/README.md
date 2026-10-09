# REM Linux 0.2 — Desktop Reference Distribution

`distribution/` is the canonical package, system-layer, and profile definition
for REM 0.2. Desktop and Fold images share package manifests and install trees.
Only profile configuration and the kernel/QEMU deployment tuple differ.

The first migration imports the read-only desktop reference image pinned in
`manifests/desktop-seed.json`, then applies declared package payloads and
removes BusyBox. The seed is never modified. Generated staging trees, ext4
images, and QEMU logs belong under `/tmp` by default.

Directories:

- `packages/`: package source, install trees, and manifests.
- `manifests/`: seed and profile package selections.
- `rootfs/`: distribution-owned base configuration overlay.
- `init/`: small REM init/getty/login/shutdown sources.
- `config/`: hostname, account policy, inittab, and shared system config.
- `scripts/`: deterministic build, import, and BusyBox audit tooling.
- `tests/`: desktop boot and in-guest qualification.
- `profiles/desktop`, `profiles/fold`: shared package selections plus target
  configuration deltas. Only desktop is a qualification target in 0.2.

See [`docs/REM-BUSYBOX-REMOVAL-AUDIT.md`](../docs/REM-BUSYBOX-REMOVAL-AUDIT.md)
and [`docs/REM-PACKAGE-FORMAT.md`](../docs/REM-PACKAGE-FORMAT.md).

## Desktop reference commands

The builder reads `REM-FLIGHT-HUMAN-UAT-FULL/rootfs.ext4` without modifying it,
checks its pinned SHA-256, imports declared package payloads, compiles the REM
system/network layer, audits BusyBox, and writes only under `/tmp`:

```sh
./distribution/scripts/build-desktop-image.sh /tmp/rem-linux-0.2-desktop-v3 1536
```

The generated image is `/tmp/rem-linux-0.2-desktop-v3/rootfs.ext4`. Boot it with:

```sh
REM_DESKTOP_ROOTFS=/tmp/rem-linux-0.2-desktop-v3/rootfs.ext4 ./distribution/scripts/boot-desktop.sh
```

Run the desktop serial qualification with:

```sh
./distribution/tests/qualify-desktop.sh /tmp/rem-linux-0.2-desktop-v3/rootfs.ext4 --work /tmp/rem-linux-0.2-v3-qualification
```

The deterministic build reproduced the v3 image byte-for-byte (SHA-256
`b54161df7a6814b8a2bea7c35a24fbdbbb1869f21849ebe1b566af240ffdac1d`). The
post-ext4 audit confirms BusyBox is physically absent. QEMU reaches REM PID 1,
mounts `/proc`, `/sys`, and `/dev/pts`, runs the SysV startup hook, and displays
the getty prompt. Full desktop qualification is blocked: password verification
faults in musl `crypt`; standalone grep, sed, and find binaries also fault under
the current kernel and trigger a kernel panic; hostname returns `ENOSYS`; and
the QEMU machine has no guest NIC. gawk, ncurses, and Dropbear payloads remain
pending. The isolated poweroff test passes with a clean ext4 check. The reboot
test did not reach a second boot. Do not treat this candidate as a qualified
desktop release.

The in-guest package smoke harness can be run with:

```sh
./distribution/tests/smoke-packages.sh /tmp/rem-linux-0.2-desktop-v3/rootfs.ext4
```

The v3 profile selects 21 shared package identities. The image currently has
18 installed payload/seed manifests: musl, system, shell, coreutils, grep, sed,
findutils, tar, gzip, bzip2, xz, diffutils, Vim, networking, chibicc,
binutils, Samurai, and curl/BearSSL. gawk, ncurses, and Dropbear are declared
but pending. Payload presence does not mean runtime qualification: grep, sed,
and find fault in the current REM kernel during the in-guest smoke run.
