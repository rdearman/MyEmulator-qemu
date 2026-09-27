# REM final candidate integration matrix

Updated 2026-09-27. This file is the integration gate record. Component-level
qualification is not the same as acceptance in one unified flight candidate.
No source package or `REM-FLIGHT-TTY` contents were modified.

## Current candidate

Core baseline: `/tmp/rem-flight-candidate-20260927-core-final`

Tuple: `REM-flight-20260925-desktop`. Rootfs SHA-256:
`30d1f1e2b301a60318e5b13751b96e8b1545700d80beec20ecb4579d27e2067b`.
The host image verifier passes. A prior #5 run passed identity, boot, console,
filesystem, and same-image persistence on this baseline. That run predates the
new unified-candidate gate; the current #5 harness requires
`acceptance_gate.unified: true` and will not treat this baseline as a final
flight candidate.

The core rootfs also contains stale alternate tool copies under
`/usr/myemulator2-linux-musl/bin/`. It is not the release candidate for the
updated binutils. A hash-guarded overlay has been prepared to install accepted
tools under `/root/native/bin/` and remove the stale alternate assembler,
linker, `ar`, and `ranlib`. No candidate has been built from this overlay.

| Component | Owner | Hash | Status | Accepted/rejected | Integration status |
|---|---|---|---|---|---|
| Kernel | Pinned tuple; unified kernel build owner pending | `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` | Core boot PASS; unified syscall set 48/72/133/439 not demonstrated on one kernel | Accepted for core tuple only; not accepted for unified tuple | In core baseline; requires unified runtime probes |
| Rootfs | #4 integration | `30d1f1e2b301a60318e5b13751b96e8b1545700d80beec20ecb4579d27e2067b` | Verifier PASS; legacy core acceptance PASS | Not final-accepted; stale alternate tools remain | Core baseline only; binutils overlay prepared |
| Yash | #3 | `ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2a8e` | PARTIAL; parent-shell interactive command acceptance not passed; syscall tuple mismatch remains | Not accepted | Staged outside the candidate; excluded |
| Kilo | Application validation recorded with #3 staging | `a8495a80effb8168116ff123eb0fd470ddfa357642b6320cd7eb07d95474b30f` | Independent save/reopen/reboot PASS; candidate harness timed out waiting for Ctrl-S/Ctrl-Q prompt | Rejected for this candidate pending rerun/fix | Test-only overlay and image retained for diagnosis; excluded from core selection |
| chibicc (previous) | #2 | `7b9134caaf776953d82d34c3f54dec0eb5b3f343d8ec92ff350df6ee82023a75` | Candidate compile probe failed with `MYEMU llist bad add` | Rejected | Overlay `chibicc-level5.json` is `accepted: false`; excluded |
| chibicc (fixed2) | #2 | `12490f4733b3401f810f866b389e88e46d043c916313def47480fbf9dfaa8ad4` | Staged candidate-test binary; #5 refused its image before QEMU because it lacked the unified gate | Pending; not accepted | Test-only candidate exists at `/tmp/rem-flight-candidate-20260927-chibicc-fixed2-test`; excluded from final selection |
| as | #6 | `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93` | Qualification PASS for documented assembly paths | Accepted at component level; unified rerun required | In core base at `/root/native/bin/as`; stale alternate copy is hash-pinned for removal |
| ld | #6 | `edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65` | Qualification PASS for `/root/native/bin/ld` | Accepted at component level; unified rerun required | In core base at canonical path; stale `/usr/myemulator2-linux-musl/bin/ld` is pinned for removal |
| ar | #6 | `31585d2cfa5dcdf702e4655b2ac386b7580641aaa378d2b39bde106bb4543ba7` | Current owner qualification PASS | Accepted at component level; candidate acceptance pending | Staged at `staging/binutils/ar`; overlay prepared, not built |
| ranlib | #6 | `b2f8ac9eba8000c34c1f609d1955bc8e3495532b3bf0739ad714554e36d00bd3` | Current owner qualification PASS | Accepted at component level; candidate acceptance pending | Staged at `staging/binutils/ranlib`; overlay prepared, not built |
| Samurai (`samu`) | #2 | Source archive `b782ef526d6d05b8236b5720295f6499332de7ca99894f360b3dc35b3705a9a3`; no accepted executable hash | No accepted guest executable; build-graph acceptance pending | Not accepted | Not staged/integrated; needs owner artifact and unified test |
| Persistence | #7 | No new fix artifact hash. Core test image derives from rootfs `30d1f1e2b301a60318e5b13751b96e8b1545700d80beec20ecb4579d27e2067b` | PARTIAL: same-image sync/reboot smoke PASS; clean unmount, unsynced-write survival, and guest build persistence not established | Not accepted as a #7 fix | Core behavior only; no owner overlay |

The flight projects pack is available in the repository (tree digest
`fe340ee48ef7c1cdf16351b984952a13460069aa9403bd019cfbedd5ed08a920`), but
is not part of the current unified guest image.

## #6 binutils integration handoff

The staged binaries match the exact owner-provided hashes and ELF32 MyEmulator2
machine. `/root/.profile` already sets PATH with `/root/native/bin` first; its
seed-rootfs SHA-256 is `0e4b7dac9b4d1a3b4b2796b3365c2cae2cfa356627002950882c8972a36b7350`.
The overlay pins that profile, native `as`/`ld`, and each selected tool's
content. It also pins and removes the old alternate binaries:

- `/usr/myemulator2-linux-musl/bin/as` —
  `920f3f144e3ec1d1c42b08b096f0ed4ff2c7cec8bbc90b4cb3a7a3f343255b7a`
- `/usr/myemulator2-linux-musl/bin/ld` —
  `c93c1c868944f173341cf7166109c4c47a24559f33abbff788e9c00ff3379f3b`
- `/usr/myemulator2-linux-musl/bin/ar` —
  `d74a2a6130b92a92b0a457157789b67d519651668fdf8f6a117d5a492b641a8b`
- `/usr/myemulator2-linux-musl/bin/ranlib` —
  `a33987c3051b485126bf9185c1e6c0db66cd7e526baf7412b01da38c81a280b0`

The overlay is `overlays/qualified-binutils.json`; its proposed tuple is
`tuples/flight-20260927-qualified-binutils.json`. Artifact/source hashes,
ELF machine, sizes, PATH profile, dependency hashes, and removal hashes were
checked read-only against the existing core image. This is a prepared
integration handoff, not a candidate build or acceptance result.

## Unified acceptance gate / #5 coordination

The updated harness requires `acceptance_gate.unified: true` and exact,
validated manifest entries for all roles: Yash, Kilo, chibicc, `as`, `ld`,
`ar`, `ranlib`, Samurai, and the linker script. The linker and script paths
must be `/root/native/bin/ld` and `/root/native/lib/myemulator2-user.ld`.
The current #5 convergence note says there is no unified tuple yet and directs
integration to wait for that manifest before running the full acceptance
sequence.

I attempted the already-built fixed2 test image once. #5 rejected it before
starting QEMU because it lacked `acceptance_gate.unified`; therefore this is
not an acceptance result for fixed2. The command for a future unified
candidate is:

```sh
tools/rem-flight-acceptance.py \
  --candidate-dir /path/to/unified-candidate \
  --results-dir /tmp/rem-flight-accept-unified \
  --repeat 2
```

No candidate was built after #6's update. The core candidate and prior test
images remain separate; do not use them as a substitute for the future
unified candidate.

## Rebuild plan once all accepted inputs are identified

1. Select one kernel/rootfs tuple that includes the required Yash syscall set;
   never combine the incompatible Yash and native-toolchain kernel tuples by
   pathname.
2. Select only owner-accepted Yash, Kilo, chibicc, Samurai, and #6 binutils.
   Exclude both the previously failed chibicc and all test-only binaries.
3. Add an explicit `acceptance_gate` lock for every required role and make the
   builder carry it into the candidate manifest. The builder now preserves an
   optional tuple `acceptance_gate` field.
4. Build a new candidate with the hash-locked overlay, verify it, and give that
   same directory to #5. Follow #5's order: boot, Yash, Kilo, compiler, `as`,
   `ld`, archives, Samurai, then persistence. Stop promotion on the first
   failed required phase and record later phases as pending.

The builder refuses output-directory overwrite, unaccepted overlays (except
explicit test-only trials), changed dependency hashes, stale-removal hash
mismatch, wrong modes/architectures, and incomplete required files. It records
removed stale paths and the explicit tool search path in the candidate
manifest. Candidate construction does not write to `REM-FLIGHT-TTY`.

## #2 delivery / pending integration selection (2026-09-27)

The integration-only staging area now has Codex #2's corrected native
chibicc (`e01b58f7768b9b7e9f3ac5e6ff5cb8598de13ca6d8c056a4ecfede91d2978084`,
667356 bytes, executable, ELF32 MyEmulator2) and native `samu`
(`d933c9e3cd6034acd70e2c518bea46e71c115e649952bd052747a337621b7ea9`,
441748 bytes, executable, ELF32 MyEmulator2). #2 reports `samu -h` PASS.
Neither is accepted for the flight candidate yet: `samu -n` and incremental
rebuild PASS are required before candidate creation.

Prepared overlay:
`overlays/flight-integration-pending-samu-gates.json`. It selects those
hash-pinned binaries, the previously qualified `ar`/`ranlib`, the staged Yash
binary, the staged Kilo binary, and Kilo source/guides. Chibicc, samu, Yash,
Kilo, and Kilo documentation/source entries remain `accepted: false`; the
builder will reject this overlay until the owners' gates pass and acceptance
is explicitly updated. The overlay does not select the previously failed
chibicc. No candidate was built.

Pinned Yash tuple descriptor:
`tuples/flight-Yash-unified-pending-samu-gates.json` selects QEMU SHA-256
`0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`, kernel
`5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a`, and
rootfs `8649389a39aefa45d9a1ed0b2b8438cde5396cbb1fe1991ee26d4633cff48d59`.
The tuple deliberately records `acceptance_gate.unified: false` and is a
selection record, not a build authorization. Kernel/rootfs inputs currently
refer to the documented disposable `/tmp` validation artifacts; preserve or
re-stage those exact hashes before any later rebuild.

| Component | Owner | Hash | Status | Accepted/rejected | Integration status |
|---|---|---|---|---|---|
| Yash | #3 | `ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2a8e` | Interactive behavior recorded on the pinned Yash tuple; unified candidate run pending | Tuple validation only; not candidate-accepted | Staged; selected pending unified overlay |
| Kilo | #3 / application validation | `a8495a80effb8168116ff123eb0fd470ddfa357642b6320cd7eb07d95474b30f` | Separate save/reopen/reboot evidence; retest on selected tuple pending | Not candidate-accepted | Binary, licensed source, and guides staged |
| chibicc (corrected) | #2 | `e01b58f7768b9b7e9f3ac5e6ff5cb8598de13ca6d8c056a4ecfede91d2978084` | Delivered; Samurai dry-run/incremental gates pending | Not accepted yet | Hash-pinned in pending overlay |
| samu | #2 | `d933c9e3cd6034acd70e2c518bea46e71c115e649952bd052747a337621b7ea9` | `-h` PASS; `-n` and incremental rebuild pending | Not accepted yet | Hash-pinned in pending overlay |
| as / ld / ar / ranlib | #6 | See entries above; `ar` `31585d2c…4543ba7`, `ranlib` `b2f8ac9e…e36d00bd3` | Qualified component artifacts | Component-accepted; same-candidate gate pending | `ar`/`ranlib` selected; canonical `as`/`ld` pinned by Yash tuple |

Candidate creation remains on hold until both Samurai gates pass. Afterward,
refresh exact artifact provenance/hashes, set only owner-accepted entries to
accepted, pin the unified gate, build once from this selected tuple, verify it,
and give that same candidate directory to #5.

## Qualified native Make overlay

The pending integration overlay now installs Codex #6's qualified GNU Make
4.4.1 as `/usr/bin/make`, replacing the seed-rootfs Make. SHA-256:
`f474110061701a845f11bee32fe09a167b8334fb088940ad33f64452a4630527`,
size 1,138,772 bytes, mode `0755`, ELF32 MyEmulator2. The owner qualification
reports C+assembly build/run, clean, and source-triggered rebuild PASS on its
disposable validation tuple. It remains a component-level acceptance; rerun it
on the selected unified Yash candidate. Samurai remains the tuple's primary
build executor when accepted; Make is included as a qualified fallback. No
candidate was built.
