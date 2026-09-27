# REM Fold flight image readiness

**Audit date:** 2026-09-27  
**Image audited:** `REM-FLIGHT-ANDROID-MVP/rootfs/rem-rootfs.ext4`  
**Result:** package contents verified read-only; the image is not yet a verified
REM development machine and has not been booted on a physical Fold.

## Audit method and image identity

The rootfs SHA-256 is
`46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd` (10 GiB).
Its package manifest agrees. `e2fsck -fn` on the offline image completed all
five passes without reporting errors: 11,940/655,360 files and 188,791/2,621,440
blocks allocated.

Paths, file modes, and binary hashes below were read from ext4 with read-only
`debugfs stat`/`dump`. Runtime probes booted this exact rootfs as the backing
image of a disposable qcow2 overlay using desktop QEMU 9.2.0, machine
`myemulator32`, 128 MiB, and the package kernel. The ext4 source image and
Android package were not written by those probes. The Fold Android QEMU binary
itself has not been run on a physical device.

| Package component | Path | SHA-256 |
|---|---|---|
| Android QEMU | `qemu/qemu-system-myemulator32` | `a6da714c1643a22e6d295967704e762369ac04dc7bca05760972e0e4b0c9e3ac` |
| REM kernel | `kernel/vmlinux` | `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` |
| ext4 rootfs | `rootfs/rem-rootfs.ext4` | `46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd` |
| Fold ZIP | `REM-FLIGHT-ANDROID-MVP.zip` | `be2d584ae115354b2bcf5dd7cafcf9cbeaa71fa68b3f4ec693a817f5b17dd84d` |

The exact QEMU, kernel, and rootfs hashes are recorded in
[`REM_PHONE_DEPLOYMENT.md`](REM_PHONE_DEPLOYMENT.md). Package checksum and
Termux instructions are in [`../android/TERMUX_DEPLOYMENT.md`](../android/TERMUX_DEPLOYMENT.md).

## Status key

- **READY** — the named workload passed on this image or the exact identified
  binary and configuration.
- **WORKS WITH LIMITATIONS** — only the stated narrow command or separately
  qualified variant passed; the full intended workflow remains unverified.
- **BLOCKED** — the packaged binary is present but fails a relevant probe, or
  a required workflow dependency/acceptance is missing.
- **NOT INCLUDED** — no binary exists at the inspected package paths.

## Shell and editor inventory

| Component | Status | Packaged path, hash, and observed result |
|---|---|---|
| BusyBox ash (`/bin/sh`) | READY as fallback shell | `/bin/sh` symlinks to `busybox`; `/bin/busybox`, mode 0755, SHA-256 `d9e89176978371aa890af591b45e26823e1d0e39005f08ade175fd23bb05fa3d`. Exact image boot reached the ash prompt and accepted the audit commands. |
| Yash | NOT INCLUDED | No `/usr/bin/yash` or `/bin/yash`. Separately validated/staged Yash is not in this ext4 image; see [`REM_YASH_STATUS.md`](REM_YASH_STATUS.md). |
| Bash | BLOCKED | `/usr/bin/bash`, mode 0755, SHA-256 `393c6188fea2776d0b91579c879c0bc72dacf53e34593295dab9ffc299731a43`. `bash --version` passed, but `bash -c 'echo BASH_COMMAND_OK'` segfaulted with status 139. Kernel fault: PC `0x020c08d4`, address `0x218`, PID 26. A different, controlled Bash build passed a narrower matrix; it is not this packaged binary. See [`REM_BASH_COMPATIBILITY.md`](REM_BASH_COMPATIBILITY.md). |
| Kilo | NOT INCLUDED | No `/usr/bin/kilo`, `/usr/local/bin/kilo`, or `/root/kilo`. Its edit/save/reboot result is from a separate disposable image, not this package. |
| Vim | WORKS WITH LIMITATIONS | `/usr/bin/vim`, mode 0755, 2,853,548 bytes, SHA-256 `4f470d902cd099b9dd78c0d498f219d7b7e05c2aca526d6a2679233f73ad1806`. Prior disposable probe passed `vim --version` but stalled before a visible buffer; edit/save is unverified. See [`REM_VIM_COMPATIBILITY.md`](REM_VIM_COMPATIBILITY.md). |

## Development tool inventory

### Binutils integration follow-up (2026-09-27)

The selected desktop seed image
`.rem-phone-deploy-20260925/REM-FLIGHT/disk/rem-rootfs.ext4` currently hashes
to `04eaef9b630891818ff5dfeaa4b525465da345057f606158eacae01f0245d906`, while
the integration tuple pins
`46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd`. The
builder rejected that path. The byte-identical rootfs copy at
`REM-FLIGHT-ANDROID-MVP/rootfs/rem-rootfs.ext4` was then verified against the
pinned SHA and selected by the binutils tuple. The candidate
`/tmp/rem-flight-binutils-final-20260927` built and passed host verification;
its rootfs SHA-256 is
`a107a3b637837a4673be28688b44824f8a7765c1b14e660e32143ac343cc5d12`.
It booted Linux 6.12.1, mounted ext4 and devtmpfs, and reached the BusyBox ash
prompt over a qcow2 copy-on-write test disk. `REM-FLIGHT-TTY` was not modified.

The selected candidate stages `objdump`, `readelf`, `nm`, `size`, `strings`,
and `strip` at `/root/native/bin/`; native `ar` and `ranlib` are also selected.
The test assembled and linked a minimal REM executable with the existing
assembler and linker, then passed `readelf -h`, `objdump -d`, `nm`, `size`,
`strings`, and `strip` on that executable. `nm`, `size`, and `strings` also
passed against `/usr/bin/true`; `readelf -h` passed on that file. The stripped
result remained readable by `readelf -h`. Full `objdump -d /usr/bin/true`
under instruction-counted QEMU exceeded one minute and was stopped; large
binary disassembly performance is unverified.

The prior overlay `objdump`, `nm`, and `strip` each faulted on a REM
executable. Replacements from the existing native Binutils build passed the
focused checks. Hashes and observed fault PCs are in
[`REM_NATIVE_BINUTILS_QUALIFICATION.md`](REM_NATIVE_BINUTILS_QUALIFICATION.md).
The native-binutils package checker passes after regeneration.

The qualified overlay assembler and linker identities remain unchanged:
`as` SHA-256 `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93`
and `ld` SHA-256
`edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65`.

| Component | Status | Packaged path, hash, and observed result |
|---|---|---|
| GNU Make | WORKS WITH LIMITATIONS | `/usr/bin/make`, mode 0755, SHA-256 `2469ba64989becf572d2c7878b92f26649ca26572c9ea2cc9c12342f1f696fd2`. Exact image `make --version` reports GNU Make 4.4.1, status 0. The qualified build/rebuild test used a different overlay binary (SHA-256 `f474110061701a845f11bee32fe09a167b8334fb088940ad33f64452a4630527`); no actual build was run with the packaged copy. |
| Samurai (`samu`) | NOT INCLUDED | No `/usr/bin/samu` or `/root/samu`. A workstation GCC-built binary passed `samu -n` on a separate tuple; chibicc-built `-n` failed, and no full Samurai build is accepted. See [`REM_SAMURAI_STATUS.md`](REM_SAMURAI_STATUS.md). |
| chibicc | NOT INCLUDED | No `/usr/bin/chibicc` or `/root/chibicc`. The compiler and runtime tuple that passed `hello.c` exists only in separate staging/test images. Larger hosted C probes remain failing or unresolved. See [`REM_C_COMPILER_STATUS.md`](REM_C_COMPILER_STATUS.md). |
| Guest GCC | NOT INCLUDED | No `/usr/bin/gcc`, `/usr/bin/myemulator2-linux-musl-gcc`, or `/root/gcc`. GCC 15.2 cross compiler is a workstation tool; native GCC is not in the Fold image. |
| Default assembler | WORKS WITH LIMITATIONS | `/usr/bin/as`, mode 0755, SHA-256 `920f3f144e3ec1d1c42b08b096f0ed4ff2c7cec8bbc90b4cb3a7a3f343255b7a`. `/usr/myemulator2-linux-musl/bin/as` is the same hash. `as --version` passes (Binutils 2.46.0.20260210), but this is not the qualified staged assembler hash and actual assembly with this default copy was not tested. |
| Qualified assembler | READY for focused assembler use | `/root/native/bin/as`, mode 0775, SHA-256 `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93`; version probe passes and this hash matches the focused native-binutils qualification. Explicitly select this path. |
| Default linker | WORKS WITH LIMITATIONS | `/usr/bin/ld`, mode 0755, SHA-256 `c93c1c868944f173341cf7166109c4c47a24559f33abbff788e9c00ff3379f3b`. `/usr/myemulator2-linux-musl/bin/ld` is the same hash. `ld --version` passes, but this copy is not the qualified linker and a stale linker at this path family is known to crash on a real link. |
| Qualified linker | READY for focused linking | `/root/native/bin/ld`, mode 0755, SHA-256 `edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65`; matches the linker that passed focused link/execute qualification. Explicitly select this path. |
| `ar`, `ranlib` | WORKS WITH LIMITATIONS | `/usr/bin/ar` SHA-256 `d74a2a6130b92a92b0a457157789b67d519651668fdf8f6a117d5a492b641a8b`; `/usr/bin/ranlib` SHA-256 `a33987c3051b485126bf9185c1e6c0db66cd7e526baf7412b01da38c81a280b0`. Both `--version` probes passed, but archive operations with these packaged copies are not qualified. The separately rebuilt archive tools have different hashes and passed their archive/link/rebuild test. |
| `nm`, `objdump`, `readelf` | WORKS WITH LIMITATIONS | `/usr/bin/nm` SHA-256 `44053be004d63c1a7baac6022212156f29b9c967ff9814eccaf0057de86ee8cc`; `/usr/bin/objdump` SHA-256 `b7547055d61a954aeaaeae41240f8ef1aee0ac0456cfa43b1cbe03958091fbdc`; `/usr/bin/readelf` SHA-256 `778d9e788068b2d8b78e85bfa2cf99c1c51f78873c20939a2a8bd91eb01b2eaa`. All exact-image `--version` probes returned 0. `objdump`/`readelf` hashes match the focused qualified tools; full inspection tasks are limited to the separately recorded tests. |
| `objcopy`, `strip` | WORKS WITH LIMITATIONS | `/usr/bin/objcopy` SHA-256 `99e2f48012f84f36470cd97aa9b267563b942750687b9b162a55238e3d80637c`; `/usr/bin/strip` SHA-256 `e062c4a6b333dd6d9ccec7433648535840cf5885715723de995e8d5165c55f99`. Exact-image version probes returned 0; file transformation was not tested. |

## Required developer workflow

The requested sequence is:

```text
Yash or Bash -> Kilo -> chibicc -> as -> ld -> REM executable -> debugger
```

Current package status by stage:

| Stage | Status | Evidence |
|---|---|---|
| Shell (Yash/Bash) | BLOCKED | Yash is **NOT INCLUDED**. The packaged Bash hash and `-c` fault are recorded above. BusyBox ash is a working fallback shell, but does not make the requested Yash/Bash stage pass. |
| Kilo | NOT INCLUDED | No Kilo binary in the inspected image. Its separate-image save/reboot pass is not package acceptance. |
| chibicc | NOT INCLUDED | No chibicc binary; `/usr/lib/libgcc.a` and `/root/native/lib/libgcc.a` are also absent. |
| `as` and `ld` | WORKS WITH LIMITATIONS | Qualified copies exist at `/root/native/bin/as` and `/root/native/bin/ld` and their hashes match focused qualifications. The default `/usr/bin` copies are different hashes and have not passed a real compile/link workload. |
| REM executable | BLOCKED | A small chibicc hello executable passed on a separately staged tuple, but the current image has no accepted end-to-end project build because the shell/editor/compiler stages are missing or failing. |
| Debugger | NOT INCLUDED | No `gdb`, `gdbserver`, or `lldb` at the inspected guest paths. `/usr/bin/addr2line` exists (SHA-256 `8ce87b507088c8391544f5c1797ba052744a035ebe39f4d727b15f200ac6e764`); qualified `objdump` and `readelf` are present, but symbol/disassembly inspection is not an interactive debugger. No guest debugger workflow has passed. |

This chain is **not accepted** on the Fold image. The requirements are
conjunctive: passing `--version` for individual tools does not demonstrate
source editing, compilation, executable creation, execution, or debugging.

## Application inventory

| Application | Status | Packaged path, hash, and observed result |
|---|---|---|
| Git | WORKS WITH LIMITATIONS | `/usr/bin/git`, mode 0755, SHA-256 `10a2a175cfe775a65b030b4ddc1ebbd2f05298eaa9bfbf91c5380d73209d7ba1`. Exact-image `git --version` reported 2.48.1, status 0. Repository init/commit and reboot persistence were not tested. |
| GNU tar | WORKS WITH LIMITATIONS | `/usr/bin/tar`, mode 0755, SHA-256 `454bfc24640a258e84f97642937b83a0fc276aa406227343422a304b883473a5`. Exact-image `tar --version` reported 1.35, status 0. Archive round-trip was not tested. |
| GNU grep | WORKS WITH LIMITATIONS | `/usr/bin/grep`, mode 0755, SHA-256 `93c68731980cfdaf1eb061d6bcd783d89ef788d6181f381dfc58e347f88bab8d`. Exact-image `grep --version` (3.11) returned 0; search behavior was not tested. |
| GNU sed | WORKS WITH LIMITATIONS | `/usr/bin/sed`, mode 0755, SHA-256 `473bd1e5f07619c2d96ffeb4ee96cf5c5a06a7c9e3ebceb640aac09b91a04c58`. Exact-image `sed --version` (4.9) returned 0; transform behavior was not tested. |
| less | BLOCKED | `/usr/bin/less`, mode 0755, SHA-256 `733ce8e3cf2171944a4caf20add12036a4c6a1eb85b13ffa19a1b5fa256f958e`. Exact-image `less --version` segfaulted with status 139. Kernel fault: PC `0x02041448`, address `0x1060`. |
| Python | WORKS WITH LIMITATIONS | `/usr/bin/python3.12`, mode 0755, SHA-256 `f895ef5798267dce628316b6037062e8d9083a497520a809b93f30c66e4d0382`; `/usr/bin/python3` and `/usr/bin/python` are symlinks to versioned Python. Exact-image `python3.12 --version` reports 3.12.9, status 0; no script/stdlib workload was tested. Earlier broad-audit crashes involved provenance-mixed Python binaries and are not attributed to this hash. |
| SSH/OpenSSH | BLOCKED | Client `/usr/bin/ssh`, mode 0755, SHA-256 `849526a42b89813e4ad40c2a88b4eca64f657d7e6673cb30f0dda1a11ceddfd1`; `scp` and `sftp` are present. Exact-image `ssh -V` segfaulted with status 139 (fault PC `0x02000180`, address `0x1070`). Server `/usr/sbin/sshd` is also present, mode 0755, SHA-256 `74623a4eb79b3097aea4325586ce24da5dc9d5ed12f54525b63acd028cdff68a`, but was not runtime-tested. The MVP has no guest NIC/network stack/TCP/DNS, so SSH transport is unavailable regardless of the client fault. |

## Persistence result

The rootfs passed the current offline read-only filesystem check. The exact
rootfs hash above is also the seed identity in
[`REM_EXT4_PERSISTENCE.md`](REM_EXT4_PERSISTENCE.md): disposable copies
preserved synced multi-block files and development fixtures over repeated
host-QEMU reboot/recovery cycles; backup/restore and host file recovery passed.
That report also records loss of an unsynced recent file after immediate QEMU
termination. Guest clean unmount/poweroff, near-full/ENOSPC behavior, and
physical Fold persistence remain unverified. Never modify or back up the
rootfs while it is attached to a running QEMU.

The read-only rootfs audit found these C-link inputs: `/usr/lib/crt1.o`,
`crti.o`, `crtn.o`, `/usr/lib/libc.a`, headers, and
`/root/native/lib/myemulator2-user.ld`. It found no `libgcc.a` in
`/usr/lib` or `/root/native/lib`; the validated hosted-C tuple requires a
matching `libgcc.a`. The image therefore does not have a complete packaged
chibicc C build tuple even aside from the compiler itself being absent.

## Minimum flight development image

The current package is a bootable REM base image, **not** a ready development
image. Minimum contents for offline source development should be:

1. The already-packaged Android QEMU executable, its Termux runtime libraries,
   launcher/config, matching REM kernel, and writable ext4 rootfs.
2. One working shell (BusyBox ash is present) and one editor whose save/reopen
   path is tested in the candidate image (Kilo is qualified elsewhere but must
   be packaged; BusyBox `vi` was not qualified in this audit).
3. One matched compiler/link path: native chibicc, REM `as`, REM `ld`, CRT
   objects, musl headers and `libc.a`, matching `libgcc.a`, and the REM linker
   script. The current image has only parts of this set; no compiler or
   `libgcc.a` is installed.
4. A proven build entry point: either a qualified Make binary in the image or
   a simple checked-in build script. Do not depend on Samurai until a real
   build/execute workflow passes on the final tuple.
5. A debugger that can inspect the REM executable, or explicitly declare
   debugging unavailable. Current image has only offline symbol/disassembly
   tools; it has no accepted debugger.
6. Offline backup/recovery instructions and enough free Android private
   storage for the mutable guest image plus at least one verified backup.

## Optional applications

Git, tar, grep, sed, Python, Bash, Vim, less, and OpenSSH are not minimum
requirements for editing and building a small offline project. Git and tar
are useful for source/archive management, grep/sed for text work, and Python
for scripting, but their project-level workflows need testing. Bash, Vim and
less can be deferred while ash and a qualified editor are available. SSH and
network services can be deferred for this offline MVP; guest networking is
disabled, and SSH currently faults before connection setup.

## Remaining blockers

- No physical Galaxy Fold boot, launcher run, or storage-space check.
- No packaged Yash/Kilo/chibicc/Samurai/GCC; Kilo and chibicc are required for
  the documented interactive C-development route unless another qualified
  editor/compiler pair is selected.
- No packaged `libgcc.a`; native compiler path cannot be linked as the
  validated hosted-C tuple specifies.
- No packaged guest debugger (`gdb`, `gdbserver`, or `lldb`); `addr2line`,
  `objdump`, and `readelf` provide limited postmortem inspection only.
- `/usr/bin/as` and `/usr/bin/ld` are not the qualified copies. The qualified
  `as` and `ld` exist at `/root/native/bin/{as,ld}`; ensure the build driver
  explicitly uses them or fix the image PATH/aliases.
- Packaged Make is not the qualified overlay. Packaged `ar`/`ranlib` are not
  the replacements that passed archive workflow qualification.
- Bash `-c`, less `--version`, and ssh `-V` fail on the exact packaged
  binaries; Git/tar/grep/sed/Python have only version-level checks here.
- Clean guest shutdown and physical-device write/reboot/restore remain open.

## Recommended packaging order

1. Preserve the published ZIP and image hashes. Create a new candidate
   directory and a new offline rootfs copy; do not overwrite the only image.
2. Check the copy with `e2fsck -fn`. Mount the copy offline and install files
   with normal filesystem operations, preserving owners, symlinks, modes, and
   hashes. Do not inject files into an active or QEMU-attached filesystem.
3. Add the chosen shell/editor, then the complete matched compiler runtime
   (including `libgcc.a`) and qualified `as`/`ld`; resolve which paths the
   compiler driver invokes. Add the accepted Make fallback. Add Samurai only
   after its real build acceptance passes.
4. Include a REM-aware debugger only if a candidate binary already exists and
   can be qualified; otherwise retain the debugger blocker without starting a
   speculative port.
5. Add optional apps only after the minimum developer workflow works. Run a
   read-only contents/mode/hash manifest check, `e2fsck -fn`, and the minimum
   edit → compile → link → execute workload on a disposable QEMU overlay.
6. On the same candidate disk, sync, stop/relaunch QEMU, verify source/build
   output hashes and modes, then backup/restore and recover a source file on
   the host. Record that the current method is synchronized QEMU stop plus
   journal recovery, not guest clean-unmount acceptance.
7. Regenerate package checksums only after the candidate passes. On the Fold,
   check the ZIP, extract into Termux private storage, run `rem check`, boot,
   write/reboot/read the same disk, and copy a verified backup off-device.

## Supporting records

- [REM_PHONE_DEPLOYMENT.md](REM_PHONE_DEPLOYMENT.md)
- [REM_EXT4_PERSISTENCE.md](REM_EXT4_PERSISTENCE.md)
- [REM_VALIDATED_RUNTIME_TUPLES.md](REM_VALIDATED_RUNTIME_TUPLES.md)
- [REM_NATIVE_BINUTILS_QUALIFICATION.md](REM_NATIVE_BINUTILS_QUALIFICATION.md)
- [REM_MAKE_FALLBACK_QUALIFICATION.md](REM_MAKE_FALLBACK_QUALIFICATION.md)
- [REM_CHIBICC_BUGLOG.md](REM_CHIBICC_BUGLOG.md)
- [REM_C_COMPILER_STATUS.md](REM_C_COMPILER_STATUS.md)
- [REM_SAMURAI_STATUS.md](REM_SAMURAI_STATUS.md)
- [REM_YASH_STATUS.md](REM_YASH_STATUS.md)
- [REM_BASH_COMPATIBILITY.md](REM_BASH_COMPATIBILITY.md)
- [REM_VIM_COMPATIBILITY.md](REM_VIM_COMPATIBILITY.md)
- [REM_APPLICATION_AUDIT.md](REM_APPLICATION_AUDIT.md)
- [REM_OFFLINE_BUILD_MANIFEST.md](REM_OFFLINE_BUILD_MANIFEST.md)
