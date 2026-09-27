# REM runtime tuple convergence gate

This record describes the evidence needed to turn the current incompatible
disposable tuples into one flight candidate. A feature is not in the selected
tuple until it is integrated into that candidate and passes the candidate
acceptance run. Artifact presence in a staging directory does not count.

## Tuple evidence

Both reported tuples use QEMU
`0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`.

| Tuple | Kernel | Rootfs | Demonstrated boundary |
|---|---|---|---|
| A, Yash terminal work | `/tmp/rem-winsize-clean/build/vmlinux`, `9ca1141421cab0142a9f78c79035b8c0cb13762a8fb166a4aada6e8ef90507fb` | Yash owner reports prefix `6c9956…`; exact image path/full digest still needs a pinned manifest | Owner report says terminal/Yash behavior; runtime probe reports `faccessat` 48 and `faccessat2` 439 missing |
| B, native toolchain base | `.rem-phone-deploy-20260925/REM-FLIGHT/boot/vmlinux`, `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` | `.rem-phone-deploy-20260925/REM-FLIGHT/disk/rem-rootfs.ext4`, `46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd` | Updated dispatcher and native toolchain base; no accepted working interactive Yash image |

The current reproducible core candidate built from B is
`/tmp/rem-flight-candidate-20260927-core-final`, with rootfs SHA-256
`30d1f1e2b301a60318e5b13751b96e8b1545700d80beec20ecb4579d27e2067b`. It has
acceptance evidence for identity, boot, console, filesystem, and persistence,
but intentionally has no owner-pending application overlays.

## Component to artifact, owner, and status

| Component | Required artifact / contract | Owner | Current status |
|---|---|---|---|
| QEMU | Pinned QEMU above; same hash in A and B | #4 integration | PASS for the B core candidate |
| Kernel | One newly built kernel containing the selected dispatcher source and the syscall coverage required by Yash and the native toolchain | #4 integration / kernel build | PENDING unified build; A and B are not interchangeable |
| Rootfs | One ext4 candidate built from the B seed with only accepted, hash-pinned overlays | #4 integration | PENDING final merge; B core candidate is reproducible and passes core acceptance |
| Yash | `/tmp/rem-yash-flight-stage/usr/bin/yash`, SHA-256 `ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2a8e`; profile and interactive parent-shell/PTY contract | #3 | PENDING OWNER; staged, not accepted for candidate integration |
| Yash syscall support | Tracked source `linux/arch/myemulator2/kernel/syscall.c` maps `faccessat` 48, `faccessat2` 439, and `pselect6` 72. `/tmp/rem-winsize-clean/linux/arch/myemulator2/kernel/syscall.c` additionally maps `rt_sigsuspend` 133, but its source currently also contains 48/439 even though the reported Tuple A binary lacks them. This source/binary mismatch means neither temporary source tree alone proves runtime support. | #4 integration / kernel build, with #3's Yash trace as acceptance input | PENDING one rebuilt kernel and runtime probes for 48, 72, 133, and 439 |
| Kilo | Candidate-test overlay `tools/rem-flight-image/overlays/kilo-candidate-test.json`, binary SHA-256 `a8495a80effb8168116ff123eb0fd470ddfa357642b6320cd7eb07d95474b30f` | Application validation / #4 integration | PENDING; separate edit/save/reopen/reboot test passed, overlay remains `accepted: false` |
| chibicc | `toolchain/flight-kit-runtime/bin/chibicc`, SHA-256 `7b9134caaf776953d82d34c3f54dec0eb5b3f343d8ec92ff350df6ee82023a75`; matched `libgcc.a` SHA-256 `759fb011a7d4dfba4da8a96d17cc978485f2daf62c2ba4545180d4de40773044` | #2 | FAIL on the held candidate: native compile emitted `MYEMU llist bad add`; overlay is `accepted: false` |
| as | B rootfs `/root/native/bin/as`, SHA-256 `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93` | #6 | PASS for documented assembly qualification; rerun after tuple merge |
| ld | B rootfs `/root/native/bin/ld`, SHA-256 `edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65` | #6 | PASS for documented links; use the canonical `/root/native/bin/ld` path and rerun after tuple merge |
| ar | B rootfs native archive tool, SHA-256 `d74a2a6130b92a92b0a457157789b67d519651668fdf8f6a117d5a492b641a8b` | #6 | FAIL qualification; exclude from flight build workflow |
| ranlib | B rootfs native archive-index tool, SHA-256 `a33987c3051b485126bf9185c1e6c0db66cd7e526baf7412b01da38c81a280b0` | #6 | FAIL qualification; exclude from flight build workflow |
| Samurai | Accepted guest `samu` executable and a working build graph | #2 | PENDING OWNER; no accepted executable |
| Networking | Accepted networking-enabled QEMU/kernel/rootfs tuple plus endpoint and payload hash | #1 | PENDING OWNER; networking work has not produced a candidate overlay/tuple |
| Persistence | The candidate writable ext4 image, clean sync/stop/reboot path, and source read-back | #7 | PASS for core candidate smoke test; rerun last after all overlays |

The native toolchain staging authority is `toolchain/flight-kit-runtime/` and
its recorded tuple in `docs/REM_VALIDATED_RUNTIME_TUPLES.md`. Its existence
does not clear the candidate compile failure. The held artifact states and
dependencies are recorded in
`tools/rem-flight-image/overlays/chibicc-level5.json`.

## Required acceptance order for a unified candidate

Build one candidate from a pinned tuple manifest and reviewed overlay lock.
Record hashes for QEMU, kernel, rootfs, every selected overlay, and every
manifest guest artifact. Do not combine files from Tuple A and B by pathname
or by “latest” convention.

Run the phases in this order, stopping promotion at the first failed required
phase while still recording every later phase as pending. Before QEMU starts,
the candidate manifest must have `acceptance_gate.unified: true` and mark each
required guest tool validated with an exact hash matching its guest-artifact
entry. The linker and linker script must use `/root/native/bin/ld` and
`/root/native/lib/myemulator2-user.ld`; alternate/stale linker paths are
rejected.

1. **Boot and console:** pinned QEMU starts the pinned kernel, mounts ext4,
   executes init, reaches an input-capable console, and executes a unique
   host-sent command with a nonce response. Verify init-shell command
   execution and exported environment expansion.
2. **Filesystem:** create a nonce file and compare exact read-back contents.
3. **Yash:** verify kernel syscall probes 48, 72, 133, and 439, then launch
   `/usr/bin/yash` from the guest terminal; verify prompt,
   command execution, pipeline, redirection, environment, child execution,
   and clean exit through the parent shell.
4. **Kilo:** create/edit/save/exit, read back exact contents, reopen and verify
   them; do this before compilation so editing is accepted on the same image.
5. **Development workflow:** create C source, edit it to change its result,
   compile with pinned chibicc, assemble with canonical `as`, link with
   canonical `ld` and pinned runtime, and verify both exact outputs. Run the
   Make edit/rebuild path when a validated Make is available; otherwise record
   it as `NOT INSTALLED` or `PENDING OWNER`. Run the equivalent Samurai path
   when a validated executable is available.
6. **ar/ranlib:** create and list an archive, index it with ranlib, link a
   program against the archive using canonical ld, and verify exact output.
7. **Samurai:** verify `samu -h` usage output, run `samu -n`, build and run the
   result, change source, verify an incremental rebuild, then run the updated
   result.
8. **Runtime compatibility:** compile and run the pinned C fixture for argv,
   variadic integer arguments, pointer tables, and 64-bit add/subtract/shift.
   Each subcheck has a distinct marker and failure-layer label.
9. **Persistence:** after all above writes, capture SHA-256 hashes, sync and
   cleanly stop; reboot the same candidate image and compare hashes for the
   filesystem marker, Kilo file, and successful source/build products.

After the Samurai phase, run optional Tier 1/Tier 2 application torture cases
from `tools/rem-flight-acceptance/torture-catalog.json`. Their individual
results include binary hashes, compiler provenance, commands, subsystem,
reproducer, and any fault PC found in the captured serial log. They are
non-critical and do not prevent or skip Samurai.

For each phase, record `PASS`, `FAIL`, or `PENDING OWNER`, plus its nonce,
command output, and log path. A pending artifact must not be silently skipped
or counted as a pass. The existing host command is:

```sh
tools/rem-flight-acceptance.py \
  --candidate-dir /path/to/unified-candidate \
  --results-dir /tmp/rem-flight-accept-unified \
  --repeat 2
```

There is no unified tuple manifest yet. Run the full acceptance command only
after integration publishes that manifest and candidate directory. Until
then, preserve the B core candidate as the baseline and keep Tuple A's Yash
evidence separate.
