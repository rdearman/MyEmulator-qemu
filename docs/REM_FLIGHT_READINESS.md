# REM Fold flight image readiness

The candidate UAT procedure is [`REM_FLIGHT_UAT_CHECKLIST.md`](REM_FLIGHT_UAT_CHECKLIST.md).

## Latest RC3 UAT retest — 2026-09-29

This was a disposable diagnostic derivative, not a release candidate. It used
kernel `/tmp/rem-rc3-uat-8b949-final4-20260928/vmlinux`
(SHA-256 `8b949bc36a0e158d0bc93b82010b76199300b1f7c2306c2c775dfb406ceacb31`)
and QEMU `/tmp/rem-rc3-uat-8b949-final4-20260928/qemu-system-myemulator32`
(SHA-256 `34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`).
The diagnostic rootfs source was `/tmp/rem-rc3-current-uat-20260928/isolation/rootfs.ext4`
(SHA-256 `593effd28949105e221483b8201b267331bf03a0f1ef65d44a88171d47766bc0`);
the final same-image reboot test disk was SHA-256
`ee490a872dde4af56f0c7b2ba854ccb39248132b564f79b22a9c8deae9e57c0a`.
Both rootfs files contained test-only scripts and helpers and must not be
promoted. The distribution source and `REM-FLIGHT-TTY` were not modified.

Boot, `dev` login, the Yash prompt, and the automated Kilo edit/save/quit
interaction passed. Build-layer isolation found that Make and Samurai no-op
edges pass, `chibicc -S` passes, and both direct assembler forms pass. A
follow-up disposable test ran `as min.s -o as-no-c.o` and
`as -c min.s -o as-c.o`; both returned zero and produced byte-identical
MyEmulator2 relocatable objects (SHA-256
`f36ceb7905ac3776e749631ce0b902a4ec732eb16d3b1033af6fb6163ffb6a5f`). The
earlier `_bfd_clear_error_data` report came from the test shell's `printf`
failure after the assembler had completed, not from `as -c`. The diagnostic
rootfs still has separate chibicc hosted-header limitations; those are not
evidence of an assembler `-c` defect. See the run records under
`/tmp/rem-native-as-c-repro2/`.

Networking did not pass. The candidate's BusyBox lacks `ifconfig`, `ip`,
`ping`, `route`, `udhcpc`, and `nslookup` applets; the network init placeholder
reported `ifconfig unavailable`, and curl did not complete. The selected
kernel config enables virtio networking but forces a command line with
`ip=off` and no `virtio_mmio.device` declaration, so the guest did not expose
the QEMU virtio-net device. This needs a kernel device-discovery/configuration
delivery and network-tool packaging before another network UAT.

A disposable assembly helper called Linux syscall 81 (`sync`) and exited;
both `as` and `ld` produced it, and the helper returned status 0. After host
`e2fsck -fy` recovery, the marker survived a reboot of the same test disk and
was readable in the guest. The post-reboot filesystem check passed. This is
recovery-path persistence only: BusyBox has no `sync` or `halt` applet, and
guest halt still did not power QEMU off. The dedicated reboot syscall probe
returns `ENOSYS` for syscall 142, so orderly guest shutdown remains blocked.
All test QEMU instances were tracked, stopped by the harness, and reported
`qemu_cleanup=PASS`; the final `ps -ef | grep '[q]emu-system-myemulator32'`
check found none.

## Current UAT candidate update — RC3 — 2026-09-28

Candidate: `/tmp/rem-flight-rc3-release-20260928/` (**NOT ACCEPTED**).
It uses Codex #3's distribution rootfs
`4a180d70ad834cbf6e253ca0f9f44fb4e40486370eb26be7eed8cdc3006f3f80`, Codex
#1's kernel `7ba58a1248e6469823db7149f79dfced72a9320a713330b1bb5fce541ff862d5`,
and the staged virtio-net-capable QEMU
`34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`.
Candidate rootfs SHA-256 is
`50d77241eebbfe1dff422ef0cfab1aa7aaa03584c3332de2019efa403fd54c09`; its
manifest SHA-256 is
`fa276901cedd740d3c851808a220491c884b79d94d47fa10e5b3239ff4651ff9`.
The candidate manifest and `SHA256SUMS` record all inputs and guest artifacts.
Host verification, checksums, and `e2fsck -fn` pass.

The only guest-file overlay changes `/etc/inittab` from
`/sbin/getty -L 115200 ttyMY0 linux` to
`/sbin/getty 115200 ttyMY0 linux` (guest-file SHA-256
`d69158b7c28e55adb577382364fbf17f5237228cccb42e649ecde0e5d934ea0e`). The
distribution source image remains unchanged. RC3 boots to `/sbin/init` and
the login prompt, with no `MYEMU_USER_ALIGN` or kernel panic observed.
Username `dev` was accepted and the password prompt appeared, but no Yash
prompt followed the password submission within 120 seconds. The guest vCPU
remained busy after reading the shadow entry; the cause is not established.
Yash commands, external commands, Kilo, compiler/build/debug workflows, and
clean shutdown/persistence are therefore not accepted.

The kernel configuration enables `CONFIG_NET`, `CONFIG_INET`,
`CONFIG_VIRTIO_MMIO`, and `CONFIG_VIRTIO_NET`. This confirms network support
is configured; no guest network traffic test passed. Bash, curl, git, and SSH
are absent from the pinned distribution/qualified staging. RC3 runtime
observations and QEMU cleanup record are in its `manifest.json` and
`/tmp/rem-flight-rc3-socket.record`. Both RC3 test VMs were stopped and
verified absent; the clean release rootfs was not used as a writable test
disk.

A later, unqualified kernel binary (`/tmp/rem01-fast/vmlinux`, SHA-256
`ec75d06e…0661398b`) was also tried with the prepared workstation rootfs.
Boot/getty/login/password prompts passed; login still did not reach Yash within
120 seconds. Read-only `e2fsck` on the disposable test copy reported free
block/inode counters set to zero while its scan counted free space; no repair
was run. QEMU cleanup passed; the clean rootfs remained hash-valid. See the
detailed tuple, log path, and exact fsck counters in
[`REM_UAT_PLAN.md`](REM_UAT_PLAN.md). This kernel is not accepted.

| Component | Owner/source | SHA-256 | RC3 integration/UAT status |
|---|---|---|---|
| QEMU | Staged platform tuple | `34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85` | Included; guest virtio network traffic not tested. |
| Kernel | Codex #1 | `7ba58a1248e6469823db7149f79dfced72a9320a713330b1bb5fce541ff862d5` | Boot/init/login prompt pass; networking options enabled; no alignment panic. |
| Rootfs | Codex #3 plus candidate getty overlay | `50d77241eebbfe1dff422ef0cfab1aa7aaa03584c3332de2019efa403fd54c09` | Host verified; getty syntax fixed; login-to-Yash remains blocked. |
| Yash | Codex #3 distribution | `ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2a8e` | Included; prompt not reached. |
| Kilo / chibicc / `as` / `ld` / Make / Samurai / remdbg | Distribution and qualified binutils | See candidate `manifest.json` | Present; not run on RC3 because Yash did not start. |
| Bash / curl / git / SSH | Optional client/tool intake | — | Not in RC3; no accepted current-tuple artifacts. |

## Previous UAT candidate update — RC2 — 2026-09-28

Candidate: `/tmp/rem-flight-uat-candidate-20260928-rc2/` (**NOT ACCEPTED**).
It packages Codex #3's distribution rootfs
`4a180d70ad834cbf6e253ca0f9f44fb4e40486370eb26be7eed8cdc3006f3f80` with
kernel `f9280fc0639762330b9636b02d387d300a02fe44bb588258b89b2d24bcfaf51b`
and QEMU `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`.
The packaged rc2 rootfs hash is
`05f9443baf1d72c33443adf3a6c57a90fbfcaed39541e224b48c32bb9be13465`; the
complete candidate hashes and file provenance are in its `manifest.json` and
`SHA256SUMS`. Host-side candidate verification and checksums pass.

Boot reached ext4, devtmpfs, `/sbin/init`, SysV `rcS`, and (after removing the
unsupported getty `-L` option in a candidate-only `/etc/inittab` overlay) the
`myemulator2 login:` prompt. Login/Yash did not reach a usable shell: shell
input triggered `MYEMU_USER_ALIGN`, then a kernel panic reporting
`Attempted to kill init! exitcode=0x00000007`. External commands and the
Kilo/chibicc/Samurai/remdbg workflow therefore remain untested. Forced
termination left ext4 free-block and free-inode counters at zero until
`e2fsck -fy` repaired the disposable test image; clean shutdown/reboot
persistence is unverified.

The selected kernel has `CONFIG_NET` and `CONFIG_VIRTIO_MMIO` disabled. QEMU
reported `netdev net0 has no peer`; guest networking is unavailable. The
distribution rootfs does not contain Bash, curl, git, or an SSH client. Curl
headers and OpenSSH probes are not client executables, and old scratch-tree
binaries are not accepted artifacts for this distribution tuple. None of
those clients was built from the pinned inputs. See
[`REM_UAT_PLAN.md`](REM_UAT_PLAN.md) for test commands and per-artifact
details.

### rc2 integration readiness matrix

| Component | Owner/source | SHA-256 | Integration/UAT status |
|---|---|---|---|
| QEMU | Flight platform input | `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` | Included; starts this kernel/rootfs and reaches init. |
| Kernel | Codex #1 | `f9280fc0639762330b9636b02d387d300a02fe44bb588258b89b2d24bcfaf51b` | Included; ext4/devtmpfs/init boot, but login path panics with `MYEMU_USER_ALIGN`. Networking options are disabled. |
| Rootfs | Codex #3 source plus #4 inittab integration overlay | `05f9443baf1d72c33443adf3a6c57a90fbfcaed39541e224b48c32bb9be13465` | Host checks pass. getty requires removal of unsupported `-L`; login prompt appears, usable shell not reached. |
| BusyBox/init | Codex #3 | `1a5a31f0a29759e1edb05ede1f684e863ec78225e43996d8cc4305847bdc4e94` | Init and rcS run; getty works after the candidate-only config correction. |
| Yash | Codex #3 distribution package | `ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2a8e` | Included; no usable prompt on rc2. |
| Bash | Prior separate qualification; no rc2 artifact | — | Missing from pinned distribution and accepted staging. Not run on rc2. |
| Kilo | Distribution package | `a8495a80effb8168116ff123eb0fd470ddfa357642b6320cd7eb07d95474b30f` | Included; not run on rc2. |
| chibicc | Current distribution package | `7b9134caaf776953d82d34c3f54dec0eb5b3f343d8ec92ff350df6ee82023a75` | Included; not run on rc2. |
| `as` / `ld` | Qualified native binutils | `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93` / `edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65` | Included; not run on rc2. |
| `ar` / `ranlib` | Qualified native binutils | `31585d2cfa5dcdf702e4655b2ac386b7580641aaa378d2b39bde106bb4543ba7` / `b2f8ac9eba8000c34c1f609d1955bc8e3495532b3bf0739ad714554e36d00bd3` | Included; not run on rc2. |
| Make | Distribution package | `f474110061701a845f11bee32fe09a167b8334fb088940ad33f64452a4630527` | Included; not run on rc2. |
| Samurai | Distribution package | `98aec69f3b5810d299045010bd5a5569bae1e3102032ddf77f2c89cf1064e15f` | Included but source distribution manifest marks it unqualified; not run on rc2. |
| remdbg | Distribution package | `d7429a2d1d8b874978e785e29c51a4623db4a5679ce90bc1691091d0136e2f55` | Included; prior qualification used a separate tuple; not run on rc2. |
| Networking | Codex #1 kernel; #5/#7 clients | — | Unavailable with rc2 kernel (`CONFIG_NET`, `CONFIG_VIRTIO_MMIO`, `CONFIG_VIRTIO_NET` absent); QEMU reports no peer. |
| curl / git / SSH client | #5/#7 when qualified | — | Not in rc2 or accepted staging; no matching build inputs. SSH remains optional. |
| User creation | Codex #3 account tools | — | Account database includes `dev`; creating an additional user was not tested because no usable login shell was reached. |
| Persistence | Codex #7 | — | Not tested: the kernel panic prevented clean shutdown/reboot. |

The source rootfs input is pinned separately in the candidate manifest as
`4a180d70ad834cbf6e253ca0f9f44fb4e40486370eb26be7eed8cdc3006f3f80`; the
candidate rootfs hash above includes the documented `/etc/inittab` overlay.
Do not promote any `not run`, `unqualified`, or `missing` row based on a test
from another rootfs/kernel tuple.

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
| Debugger | NOT INCLUDED IN THIS FOLD IMAGE | No `remdbg`, `gdb`, `gdbserver`, or `lldb` at the inspected guest paths. `/usr/bin/addr2line` exists (SHA-256 `8ce87b507088c8391544f5c1797ba052744a035ebe39f4d727b15f200ac6e764`); qualified `objdump` and `readelf` are present, but symbol/disassembly inspection is not an interactive debugger. This package-specific audit does not contradict the separately passing isolated `remdbg` qualification documented in [`REM_DEBUGGER.md`](REM_DEBUGGER.md); the Fold image itself has not passed debugger workflow acceptance. |

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

## Integrated developer candidate (2026-09-28)

Candidate inputs and selected component hashes are pinned in
[`rem-flight-next-20260928.json`](../tools/rem-flight-image/manifests/rem-flight-next-20260928.json).
The reproducible input tuple uses QEMU
`34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`, kernel
`bde7533833a49357d80b934eb2c92d6cc61c75dca50803a9a76160011b6ef94a`, and
seed rootfs `a8443f8b3cc8ead472b0be67cd01bb5f9c32f42116834a3ce308f1b90d8c24bd`.
The built image is `/tmp/rem-flight-next-20260928/rem-rootfs.ext4`, SHA-256
`776b38adc30a7db9803ae42bdee9cd2fe28e47c9ecf4b40c4f384e7179dd3cce`.
Its separate QEMU/kernel/rootfs hashes are listed in
[`rem-flight-next-20260928.SHA256SUMS`](../tools/rem-flight-image/manifests/rem-flight-next-20260928.SHA256SUMS);
guest binary hashes and acceptance flags are in
[`rem-flight-next-20260928.guest-SHA256SUMS`](../tools/rem-flight-image/manifests/rem-flight-next-20260928.guest-SHA256SUMS).

Host assembly, SHA/mode/ELF checks, required-file checks, device-node checks,
and `e2fsck -fn` passed. The builder verified 45 guest files. The candidate
contains Bash and Yash, Kilo, chibicc fixed2, NO_POSIX_SPAWN Samurai, Make,
qualified native binutils, remdbg, the matching staged `libgcc.a`, and the
Kilo/debugger sources and guides. `grep`, `sed`, and `tar` resolve to BusyBox
applets; Python 3.12 is present from the seed image.

**Integration status: NOT ACCEPTED.** Boot attempts on the candidate image's
separate writable copies emitted repeated `MYEMU_TTYTRACE switch` records and
did not reach the interactive shell prompt. The debugger qualification runner
timed out waiting for `# ` after 150 seconds; retrying with 128 MiB produced
the same result. No edit/save, persistence, compile/build/run, or debugger
workflow is claimed for this assembled image. The diagnostic does not
establish the cause of the boot loop. No QEMU instances remain running, and
only disposable boot-copy images were removed.

The shell, editor, compiler, Samurai, debugger, and runtime additions remain
`candidate_test_only` in the manifest; individually passing tests on separate
tuples do not qualify this unified image. The binutils and Make selections
retain their previously qualified hashes. Do not use this candidate as the
flight image until boot and the same-image workflows pass. Full results are in
[`REM_FLIGHT_CANDIDATE_20260928_QUALIFICATION.md`](REM_FLIGHT_CANDIDATE_20260928_QUALIFICATION.md).

## Fold preparation candidate rebuild (2026-09-28)

The latest hash-pinned assembly is `/tmp/rem-flight-fold-candidate-20260928`.
Its manifest is
[`rem-flight-fold-candidate-20260928.json`](../tools/rem-flight-image/manifests/rem-flight-fold-candidate-20260928.json)
and its host checksum list is
[`rem-flight-fold-candidate-20260928.SHA256SUMS`](../tools/rem-flight-image/manifests/rem-flight-fold-candidate-20260928.SHA256SUMS).
The 10 GiB ext4 rootfs SHA-256 is
`6827e9f8c547952b6ee7c32ea2a61e33b907903175c4b11cd56a680234164b61`.
Its inputs are QEMU `34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`,
kernel `bde7533833a49357d80b934eb2c92d6cc61c75dca50803a9a76160011b6ef94a`,
and seed rootfs `a8443f8b3cc8ead472b0be67cd01bb5f9c32f42116834a3ce308f1b90d8c24bd`.

Rebuild and host-check commands:

```sh
python3 tools/rem-flight-image/build.py \
  --output /tmp/rem-flight-fold-candidate-20260928 \
  --tuple tools/rem-flight-image/tuples/flight-20260928-integration-candidate.json \
  --artifacts tools/rem-flight-image/overlays/flight-20260928-integration-candidate.json
python3 tools/rem-flight-image/verify.py /tmp/rem-flight-fold-candidate-20260928
(cd /tmp/rem-flight-fold-candidate-20260928 && sha256sum -c SHA256SUMS)
```

The builder passed `e2fsck -fn`, checked the selected file hashes, modes,
ELF machines and device nodes, and `verify.py` verified 54 guest files.
The image contains both Bash and Yash, Kilo, chibicc fixed2, samu, Make,
qualified native binutils, remdbg, and static rsync 3.5.1. The integration
overlay also installs the linker script at chibicc's default
`/lib/myemulator2-user.ld` path; the candidate includes its hash in the
manifest. Its `/sbin/init` selects Yash as login shell and BusyBox ash as
recovery. All changing shell, compiler, editor, build, debugger and rsync
selections remain `candidate_test_only`.

**Status: NOT ACCEPTED.** Runtime probes used disposable variants of the
hash-pinned inputs and exposed blockers before the full workflow could pass:

- On a Yash-first variant using the debugger kernel (`bde75338…`), Yash
  repeatedly reported `sigsuspend: Function not implemented`; disabling job
  control did not stop the failure. The recorded Yash tuple requires
  `rt_sigsuspend` (syscall 133).
- On the debugger kernel (`bde75338…`) with the package's Bash-first init, the
  debugger harness timed out waiting for the shell prompt after 150 seconds.
  The serial log contains repeated `MYEMU_TTYTRACE switch` records.
- A Bash smoke script returned `SCRIPT_REM`, and `/usr/bin/rsync --version`
  printed version 3.5.1 on a tooling-only variant. Its first rsync URL client
  invocation to `127.0.0.1` faulted at PC `0x020bee54`, address
  `0xfffffef4`, cause `8`. Loopback backup/restore did not pass.
- The first chibicc compile attempt found the missing default linker-script
  path; the corrected copy is now staged and host-verified, but a C
  compile/link/run has not passed on the latest candidate.
- Kilo started but did not draw its help screen after the serial harness
  answered the cursor-position request. The stable TTY kernel emitted ext4
  writeback warnings that pages had no attached buffers. Clean shutdown,
  reboot persistence, Kilo editing, Samurai/Make builds and remdbg interaction
  remain unqualified on one image.

The existing component-level passes are from distinct tuples and do not
promote these integrations. See
[`REM_FLIGHT_CANDIDATE_20260928_QUALIFICATION.md`](REM_FLIGHT_CANDIDATE_20260928_QUALIFICATION.md)
and [`REM_SOURCE_BACKUP.md`](REM_SOURCE_BACKUP.md) for exact observations.

Codex #6 added a reproducible in-guest build/debug smoke project at
[`tests/developer/`](../tests/developer/) and added PID/command recording plus
QEMU cleanup verification to
[`tests/debugger/run-qualification.py`](../tests/debugger/run-qualification.py).
The smoke project passes host syntax/dry-run checks but is not yet accepted on
this candidate because no shell prompt was reached; it must be run on the
final selected tuple after the init/TTY boot path is fixed.

### Boot-path integration trace (2026-09-28)

Read-only inspection of this candidate confirms `/dev/console` is a character
device `5:1` with mode `0600`, and `/sbin/init` is executable mode `0755`.
The init script launches Yash as a child with stdin/stdout/stderr redirected
to `/dev/console`, but does not create a new session or explicitly acquire a
controlling terminal. The selected BusyBox configuration also has both
`CONFIG_SETSID` and `CONFIG_CTTYHACK` disabled. Thus this package has no
BusyBox helper for the session/controlling-terminal setup already identified
as necessary by the terminal probes; runtime session state on this exact
candidate remains unobserved.

The selected kernel hash is `bde7533833a49357d80b934eb2c92d6cc61c75dca50803a9a76160011b6ef94a`.
The recorded Yash-capable kernel hash `5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a`
is no longer present at its tuple path, and the selected kernel source
dispatcher has no syscall 133 case. Existing Yash-first runtime evidence on
this kernel reports `sigsuspend: Function not implemented`. The integration
candidate therefore cannot pass interactive Yash with its currently pinned
kernel; selecting or delivering a qualified kernel with `rt_sigsuspend`
support is required before boot acceptance can continue. No QEMU instance was
started for this inspection.

### Controlled-console retest (2026-09-28)

A disposable candidate substituted a test-only C session supervisor for
`/sbin/init`. On the TTY-qualified kernel
`e2516e8af9ffc0ee6ddb2113592f98d6c65f9b376961ee99c2ab24bc5fa546de`, it
successfully forked Yash into a new session, opened `/dev/console`, acquired
the controlling terminal with `TIOCSCTTY`, and set the foreground process
group. The supervisor reported `sid=20 pgrp=20 fg=20`; an interactive Yash
prompt appeared, a builtin command returned its marker, and
`/usr/bin/bash -c 'echo FOLD_BASH_SCRIPT_OK'` returned its marker. This
demonstrates that the candidate's console/session launch gap can be addressed
in the integration layer.

Yash then repeatedly printed `sigsuspend: Function not implemented` when
starting an external tool; a chibicc invocation did not complete. The same
failure occurred with Yash job control disabled (`+m`), so controlling-terminal
setup alone does not clear the blocker. An existing backup kernel
(`vmlinux.ttytrace-backup`, SHA-256
`1490a118c809d753ebb111d374675966978a0f32d49648039a9953ce1ebee19d`)
reached the prompt but reproduced this failure. Another existing kernel
(`.linux-build/build/vmlinux`, SHA-256
`13f5145afbc229cc6c57aca25e1d77a899b543f8925fcd3e8c3d50cbbeffd5d4`)
ran its embedded `/init`, which exited and caused a kernel panic before it
mounted the candidate rootfs; it is not a viable flight selection. These
kernel substitutions were diagnostic only. The selected flight kernel still
needs a qualified `rt_sigsuspend` implementation before external commands and
the same-image workflow can be accepted.

All test QEMU instances were stopped and reaped after each run. No
`qemu-system-myemulator32` process remains. The supervisor source and binaries
are test-only files under
[`tools/rem-flight-image/staging/integration/`](../tools/rem-flight-image/staging/integration/);
they are not an accepted production init replacement.

### Developer build/debug smoke assets (2026-09-28)

The test project and one-shot guest qualification driver are in
[`tests/developer/`](../tests/developer/). The Makefile and `build.ninja`
both spell out chibicc assembly generation, native assembly, and REM-script
linking; the qualification also checks a fresh remdbg breakpoint/step/inspect
session and the staged fault fixture. Host shell syntax checks and Make/Ninja
dry runs pass. The guest build/debug workflow is **not PASS**: the current
stock assembler faults on repeated chibicc output and native linker
re-invocation is unreliable, as detailed in
[`REM_DEBUGGER.md`](REM_DEBUGGER.md#native-build-and-debugger-integration-reducer-2026-09-28).
The exact staged debugger artifacts remain hash-pinned by
[`remdbg-developer-candidate.json`](../tools/rem-flight-image/overlays/remdbg-developer-candidate.json),
but the overlay has not passed this end-to-end build workflow on the final
flight image. No production image was modified for the reducer.

### REM distribution rootfs assembly (2026-09-28)

The disposable distribution candidate is assembled at
`/tmp/rem-distribution-candidate-20260928/`; its ext4 hash, artifact hashes,
and qualification notes are in the adjacent `manifest.json` and
`SHA256SUMS`. The 512 MiB ext4 image passes offline `e2fsck -fn`. It contains
the SysV init layout, BusyBox init/getty/login candidate, root and `dev`
accounts, Yash profile, developer toolchain, rempkg, and offline docs. This
was an assembly check only: no QEMU instance was started, and boot, getty,
login, environment export, guest compilation, and package installation have
not been verified on this rootfs.

Known tuple limits remain: the selected TTY kernel
(`e2516e8af9ffc0ee6ddb2113592f98d6c65f9b376961ee99c2ab24bc5fa546de`) lacks
`rt_sigsuspend` syscall 133, which blocks Yash external commands, and the
BusyBox init/login candidate has not been run in this image. The rootfs is
not release conformant to the one-validated-toolchain-tuple policy because
some independently qualified static components were built by different
toolchains. `samu` is specifically marked experimental/unqualified. The
manifest identifies all included native binary hashes and statuses; no
production flight image or kernel was changed.
