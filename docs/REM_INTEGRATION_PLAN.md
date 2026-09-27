# REM final flight integration plan

**Status:** preparation only. Do not build or promote a final candidate until
Samurai passes `samu -n`, a real build and incremental rebuild on the selected
tuple, and Codex #5's unified candidate gate. No production `REM-FLIGHT-TTY`
files are inputs to an in-place update; every build output must be a new,
disposable directory.

## Selected tuple and current evidence

The prepared Yash tuple is
`tools/rem-flight-image/tuples/flight-Yash-unified-pending-samu-gates.json`.
It pins these inputs:

| Input | SHA-256 | Status |
|---|---|---|
| QEMU, `REM-FLIGHT-TTY/qemu/desktop/qemu-system-myemulator32` | `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` | Pinned host executable |
| Kernel, `/tmp/rem-yash-unified-build/vmlinux` | `5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a` | Yash interactive tuple; keep paired with its rootfs |
| Rootfs, `/tmp/rem-yash-qual/flight-yash.ext4` | `8649389a39aefa45d9a1ed0b2b8438cde5396cbb1fe1991ee26d4633cff48d59` | Yash runtime base; read-only inspected for packaging inputs |

The kernel and rootfs currently reside in `/tmp`; preserve these exact hashes
or re-stage verified copies before building. The tuple has
`acceptance_gate.unified: false` and is not build authorization.

## Required components and artifact pins

| Component | Owner/source | SHA-256 | Current state / integration location |
|---|---|---|---|
| Yash | #3; staged `tools/rem-flight-image/staging/yash/yash` | `ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2a8e` | Interactive behavior evidenced on its tuple; same-candidate acceptance pending; `/usr/bin/yash` |
| Kilo | Application validation; staged `staging/applications/kilo` | `a8495a80effb8168116ff123eb0fd470ddfa357642b6320cd7eb07d95474b30f` | Prior save/reopen/reboot evidence; retest on selected tuple; `/usr/bin/kilo` |
| Kilo source | Antirez Kilo 0.0.1 snapshot, `staging/applications/kilo-source/kilo.c` | `1335fa65bc121d0b4b25ec9ad4c128aa63563adb7d133dd72191ffd52c0943f6` | Staged with source license header; proposed `/usr/share/src/kilo/kilo.c` |
| Kilo guide | `staging/applications/kilo-docs/KILO_GUIDE.md` | `6c086c7e5ac8a8c21d153fe62d27b8c17f8d64d9ba79f7c65cbf4b3bc2964507` | Proposed `/usr/share/doc/rem-flight/KILO_GUIDE.md` |
| Kilo source guide | `staging/applications/kilo-docs/KILO_SOURCE_GUIDE.md` | `e310df4b6900fcda71413b40366313dfe6e3e5611ec05552683e14784a746a6b` | Proposed `/usr/share/doc/rem-flight/KILO_SOURCE_GUIDE.md` |
| chibicc | #2; staged `staging/chibicc-samurai/chibicc-e01b58f` | `e01b58f7768b9b7e9f3ac5e6ff5cb8598de13ca6d8c056a4ecfede91d2978084` | Corrected delivery; pending the Samurai gates and unified acceptance; `/root/native/bin/chibicc` |
| as | #6; validated runtime package | `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93` | Component-qualified; `/root/native/bin/as` |
| ld | #6; validated runtime package | `edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65` | Component-qualified; `/root/native/bin/ld` |
| ar | #6; `staging/binutils/ar` | `31585d2cfa5dcdf702e4655b2ac386b7580641aaa378d2b39bde106bb4543ba7` | Component-qualified; `/root/native/bin/ar` |
| ranlib | #6; `staging/binutils/ranlib` | `b2f8ac9eba8000c34c1f609d1955bc8e3495532b3bf0739ad714554e36d00bd3` | Component-qualified; `/root/native/bin/ranlib` |
| Samurai (`samu`) | #2; staged `staging/chibicc-samurai/samu-d933c9e` | `d933c9e3cd6034acd70e2c518bea46e71c115e649952bd052747a337621b7ea9` | `-h` reported PASS; `-n`, real build, and incremental rebuild not accepted; `/root/native/bin/samu` |
| Native linker script | Validated runtime package `toolchain/flight-kit-runtime/lib/myemulator2-user.ld` | `6ec14dea3bda5ded2ef8b23ac1995d6b40857e2273ead8e095d57f25e8a54621` | Staged by pending overlay; `/root/native/lib/myemulator2-user.ld` |
| musl libc | Selected Yash rootfs `/usr/lib/libc.a` | `30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924` | Present in pinned rootfs; matched runtime tuple |
| CRT `crt1.o` | Selected Yash rootfs `/usr/lib/crt1.o` | `9951b664ec350b8ae34c30be9824a42b3a76253978515d674bf1d33b346a4697` | Present in pinned rootfs; matched runtime tuple |
| CRT `crti.o` | Selected Yash rootfs `/usr/lib/crti.o` | `7fb830f63e7ba115f433b6f28253e8510bbc7b0ab0708c62a2a0ac94aeba5f37` | Present in pinned rootfs; matched runtime tuple |
| CRT `crtn.o` | Selected Yash rootfs `/usr/lib/crtn.o` | `5594cc2bd0fa13d22e159c78c1f3065b7f36ae217badcdeaf8e71b5c3ad64937` | Present in pinned rootfs; matched runtime tuple |
| REM libgcc | `toolchain/flight-kit-runtime/lib/libgcc.a` | `759fb011a7d4dfba4da8a96d17cc978485f2daf62c2ba4545180d4de40773044` | Added by pending overlay at `/root/native/lib/libgcc.a`; required for hosted link |
| GNU Make 4.4.1 | #6; `toolchain/native-make-flight-overlay/bin/make` | `f474110061701a845f11bee32fe09a167b8334fb088940ad33f64452a4630527` | Qualified build/clean/touch-rebuild path; pending overlay installs it as required executable `/usr/bin/make`. Preferred executor remains `samu` |
| GCC driver | No selected artifact | — | Not included or required for the chibicc-native path. `libgcc.a` is a compiler runtime archive, not a GCC driver. |

All rows above are artifact identity/provenance pins, not unified flight
acceptance. In particular, do not mark chibicc or Samurai accepted solely
because the files exist or `samu -h` works. The most recent checked
`docs/REM_SAMURAI_STATUS.md` still says the corrected variadic probe needs a
clean guest validation before `samu -n` can be called PASS. Obtain a newer
owner result that supersedes that recorded failure before proceeding.

The selected rootfs contains the exact libc and CRT hashes listed above but
does not contain `/root/native/lib/libgcc.a`; the pending overlay supplies the
qualified archive and linker script. The tuple PATH starts with
`/root/native/bin`, so native tools take precedence. Do not add or select the
stale `/usr/myemulator2-linux-musl/bin/ld`.

## Integration order

1. **Freeze inputs:** check all QEMU/kernel/rootfs and owner artifact hashes,
   modes and ELF32 MyEmulator2 identity. Preserve the matched Yash kernel and
   rootfs as a pair; do not substitute the older desktop core tuple.
2. **Gate Samurai before image construction:** on the exact corrected
   chibicc/binutils/runtime/Samurai set, obtain `samu -n PASS`, real build and
   run PASS, and edit-triggered incremental rebuild PASS. Capture the exact
   binary hashes, build graph and logs in owner evidence.
3. **Review selections:** update
   `overlays/flight-integration-pending-samu-gates.json` only from owner
   evidence. Keep failed or unverified artifacts `accepted: false`; never
   select the old failed chibicc or stale linker. Pin dependencies and the
   libc/CRT/runtime linker script hashes.
4. **Set the unified lock:** update the tuple with the selected hashes and
   paths for all #5-required roles (`yash`, `kilo`, `chibicc`, `as`, `ld`,
   `ar`, `ranlib`, `samu`, linker script, libc, libgcc and CRT). Set
   `unified`/`validated` only when the selected manifest is complete and
   owner gates are satisfied.
5. **Build once into a new path:** run `tools/rem-flight-image/build.py`
   with that tuple and overlay. Do not reuse or overwrite an output directory.
6. **Verify:** run `tools/rem-flight-image/verify.py CANDIDATE`. Check the
   manifest, artifact hashes/modes/architectures, dependency pins, removed
   stale paths, device nodes, ext4 check and image identity.
7. **Accept that same candidate:** run the #5 acceptance harness against that
   exact candidate directory. Require boot/console/filesystem, Yash, Kilo,
   compiler-to-executable, assembler/linker, archive, Samurai dry-run/build/
   incremental rebuild and restart-persistence phases to pass. Do not rebuild
   or mutate the candidate between verification and acceptance.
8. **Promote by copying only after PASS:** retain candidate manifest, logs,
   tuple and overlay beside the output. Production-package promotion is a
   separate explicit step; the builder never writes to `REM-FLIGHT-TTY`.

## Validation and failure handling

Use these commands after the Samurai gate and unified lock are ready (not now):

```sh
python3 tools/rem-flight-image/build.py \
  --tuple tools/rem-flight-image/tuples/flight-Yash-unified-pending-samu-gates.json \
  --artifacts tools/rem-flight-image/overlays/flight-integration-pending-samu-gates.json \
  --output /tmp/rem-flight-unified-YYYYMMDD
python3 tools/rem-flight-image/verify.py /tmp/rem-flight-unified-YYYYMMDD
tools/rem-flight-acceptance.py \
  --candidate-dir /tmp/rem-flight-unified-YYYYMMDD \
  --results-dir /tmp/rem-flight-accept-YYYYMMDD --repeat 2
```

The current overlay deliberately has candidate creation disabled in its
selection metadata and leaves pending components unaccepted; the builder
rejects them. Do not flip those fields to bypass a gate. After any failed
build or verification, preserve its logs and manifest if present, discard
only that newly created disposable candidate, then correct inputs/metadata
and rebuild to a distinct output path. On acceptance failure, keep the
production package and prior validated candidate untouched, record failing
phase, and return to the relevant owner with the evidence. Rollback is a
selection change to the last known-good pinned tuple/candidate, never an
in-place mutation of the rootfs or production package.

## Existing packaging scripts and gaps

Available mechanisms:

- `tools/rem-flight-image/build.py` — hash-pinned tuple assembly and safe
  overlay construction; refuses overwrite and unaccepted artifacts.
- `tools/rem-flight-image/verify.py` — verifies host inputs, candidate ext4,
  guest hashes/modes/architectures, dependency hashes and devices.
- Candidate `boot.sh` — verifies before launching the candidate's matched
  QEMU and kernel.
- `tools/rem-flight-acceptance.py` — #5's acceptance runner for one candidate.

No core build, verify, or boot script is missing. There is not yet a packaging
helper that turns owner acceptance evidence into a fully populated,
role-keyed #5 `acceptance_gate` and reconciles that lock with the overlay and
required guest-file list. Until such a helper exists, that reconciliation is
a manual review step; #5's harness remains the final fail-closed check. The
prepared tuple's kernel/rootfs source paths are temporary `/tmp` paths and
must be re-staged under stable, hash-verified paths for a durable rebuild.
