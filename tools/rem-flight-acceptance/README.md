# REM flight acceptance

For current tuple conflicts, component ownership/status, and the required
unified-candidate acceptance order, see
[`TUPLE-CONVERGENCE.md`](TUPLE-CONVERGENCE.md).

Run from the repository root on a Linux x86-64 host with Python 3, QEMU's
supporting host libraries, `readelf`, `debugfs`, `e2fsck`, and `cp` available:

```sh
tools/rem-flight-acceptance.py
```

The default run uses the exact tuple pinned in
`tools/rem-flight-image/tuples/flight-20260925.json`. It verifies the QEMU,
kernel, and rootfs SHA-256 values before starting a VM. Never replace one of
those files in place while retaining the tuple; update or select a manifest
that pins the new artifact hashes.

The runner copies the rootfs to a unique directory under `/tmp`, checks the
filesystem and required files, then adds only explicitly hash-pinned accepted
artifacts from `artifacts.json` to that disposable copy. It records hashes of
important guest files, QEMU arguments, per-iteration status, and serial logs.
The source flight image is hash-checked again after the run. Nothing is written
back to the flight image. Reports are under `/tmp/rem-flight-accept-*`.

Useful options:

```sh
tools/rem-flight-acceptance.py --repeat 2
tools/rem-flight-acceptance.py --candidate-dir /path/to/flight-image-candidate
tools/rem-flight-acceptance.py --tuple /path/to/pinned-tuple.json --timeout 180
```

Candidate directories must contain the flight-image builder's `manifest.json`
and `qemu-system-myemulator32`, `vmlinux`, and `rem-rootfs.ext4` files. Candidate
manifests provide artifact hashes and machine settings. A candidate's
integrated artifacts must be declared in its own manifest; held or test-only
overlays are not accepted by the runner.

Candidate and tuple manifests must also contain an `acceptance_gate` lock before
the runner starts QEMU. It must set `unified` to `true` and pin the accepted
guest tools and linker script. Each role entry has `path`, lowercase 64-digit
`sha256`, and `validated: true`; the same path/hash must appear in
`guest_artifacts`, whose contents are checked against the rootfs before boot.
Required roles are `yash`, `kilo`, `chibicc`, `as`, `ld`, `ar`, `ranlib`,
`linker_script`, `libc`, `libgcc`, `crt1`, `crti`, and `crtn`. `make`, `samu`,
and Vim are optional roles: when present and selected for runtime coverage,
they must also have validated hash entries. The
toolchain paths are fixed to
`/root/native/bin/{chibicc,as,ld,ar,ranlib}` and the script to
`/root/native/lib/myemulator2-user.ld`; libc and CRT paths are pinned under
`/usr/lib`, with `libgcc.a` at `/root/native/lib/libgcc.a`. A missing unified
marker, stale path, unvalidated tool, or mismatched tool hash is rejected
before QEMU execution.

The runtime checks run boot, bidirectional console, init-shell command and
environment, filesystem read-back, Yash, Kilo edit/save/read-back, chibicc
source create/edit/compile, direct `as` object creation, direct `ld` link and
execution, optional Make rebuild, `ar`/`ranlib` archive and link, Samurai
help/dry-run/build/incremental rebuild, then a final sync and reboot. The final
reboot compares SHA-256 hashes of the filesystem marker and successful editor,
source, and build outputs on the same disposable image. Link tests use only
the canonical paths and pinned CRT/libc/libgcc entries; compiler output alone
does not count as a successful compile/run chain.

## Acceptance tests and failure boundaries

| Test | What a pass proves | Expected failure boundary |
|---|---|---|
| Boot | Pinned QEMU loads the pinned kernel, mounts ext4, runs init, and reaches the input-capable shell prompt. | Kernel/init panic or QEMU exit is `FAIL`; timeout before prompt is `FAIL`. |
| Console and shell | A host-sent nonce command runs and returns its exact response; the init shell executes a command and expands an exported environment value. | Printed boot text without command response does not pass; input/echo/expansion mismatch is `FAIL`. |
| Filesystem | Guest creates a unique file and reads back the exact expected contents. | Create, read, or content mismatch is `FAIL`. |
| Yash | When validated and installed, interactive prompt, command, environment, pipeline, redirection, child execution, and clean exit work. | Missing/unvalidated Yash is `PENDING OWNER`; observed interaction mismatch is `FAIL`. |
| Kilo | Kilo edits and saves a unique buffer; exact file contents can be read from the persistent filesystem. | Missing app is `NOT INSTALLED`; failed save/read-back is `FAIL`. |
| Vim | Validated Vim starts, writes a nonce line through an edit command, saves, and reads back exactly one expected line. | Missing Vim is `NOT INSTALLED`; installed without a validated hash is `PENDING OWNER`; startup/save/read-back failure is `FAIL`. |
| Native development | A C source is created, edited to change its output, compiled by pinned chibicc, assembled by pinned `as`, linked by pinned `ld`, and both runs match expected output. | Compiler, assembler, linker, runtime, or output failure is `FAIL`; stale paths/hash mismatch are rejected before QEMU. |
| Runtime ABI | A native executable checks argv, variadic integer arguments, string pointer tables, and 64-bit add/subtract/shift; each condition has its own output marker and failure return code. | Exit codes 11–14 identify argv, variadic, pointer-table, and 64-bit failure layers; fatal traps/unknown failures remain captured in serial output. |
| Make | If installed and hash-validated, Make builds and runs the C program, then notices an edit and rebuilds it to changed output. | Absent Make is `NOT INSTALLED`; installed but unvalidated Make is `PENDING OWNER`; build/rebuild mismatch is `FAIL`. |
| ar/ranlib | Pinned archive tools create/list/index a library, pinned ld links against it, and the program prints the unique expected output. | Missing/unvalidated tool is blocked by the candidate gate; command/link/output failure is `FAIL`. |
| Samurai | When installed, `samu -h` prints help, `samu -n` shows the expected build edge, build/run works, and source edit causes a verified incremental rebuild. | Missing Samurai/compiler is `PENDING OWNER`; parse/build/rebuild/output failure is `FAIL`. |
| Filesystem lifecycle | Create/read marker; create, modify/read, and delete a second file; verify deletion again after reboot. | Any operation or exact-content/deletion mismatch is `FAIL`. |
| Persistence | After all writes, the guest syncs and shuts down; after reboot the marker and successful output/source files have identical SHA-256 hashes. | Missing file, hash mismatch, ext4 check error, or reboot failure is `FAIL`. |

`tests.json` is the machine-readable test catalog. Every catalog row records
the command, expected result, failure layer, artifact roles, and runtime
artifact hashes. The runtime C fixture's SHA-256 is recorded in each ABI test
entry. Samurai remains its own phase: failures in the new Vim, runtime ABI, or
Make phases do not skip its test. Vim and Samurai run only when installed and
accepted by the candidate lock.

Only executed guest checks can receive `PASS`. Artifact presence and a prompt
alone are not runtime evidence. Reports retain tuple hashes, tool hashes,
per-phase details, nonces, QEMU arguments, and serial logs.

## Application torture workloads

The independently reported `APPLICATION TORTURE` phase runs after Samurai so
these non-critical workloads cannot suppress its test. The Tier 1 catalog
checks BusyBox applet dispatch, coreutils sort/uniq/wc pipelines, grep matching,
sed substitution, and tar create/list/extract/content recovery. Tier 2 checks
less terminal startup/page/exit, dialog ncurses drawing and Enter handling,
and git init/add/commit/status/log. Missing executables are `NOT INSTALLED`;
they are not reported as passes.

`torture-catalog.json` is the machine-readable workload definition. Each case
has a bounded command, a distinctive expected result, its subsystem, and a
minimal reproducer. The checks prove the named command path and observable
result; they do not claim broader application compatibility. For a failure,
the JSON report includes tested executable hashes and their sources, compiler
provenance (or `UNKNOWN` when the candidate metadata does not identify it),
the executed command, the failure subsystem/layer, a reproducer, captured
serial output, and a fault PC if the guest reported one. `less` must display
its page and exit after `q`; `dialog` must display its unique message and exit
after Enter. Git must create a commit, show a clean status, and return the
expected commit subject.

## Reading the result

The command prints a separate status for artifact identity, boot, bidirectional
console, init-shell behavior, filesystem, Yash, Kilo, Vim, chibicc, runtime
ABI, Make, archive tools, Samurai, networking, and final persistence.
`PENDING OWNER` means an owner-controlled artifact/acceptance hook
is not available; `NOT INSTALLED` means an optional application is absent;
`TEST ERROR` means the harness could not run or interpret that phase. A prompt
does not pass console input: the guest must execute a command and return a
nonce-derived response. Filesystem acceptance writes and reads back exact
contents, then checks the same file after a synced VM stop and a second boot.

Separate QEMU sessions isolate interactive and application checks. A timeout
or guest failure leaves the captured serial output in its result directory.
Every VM writes only to that iteration's disposable rootfs copy. Existing
running VMs are not killed; each QEMU process is launched in its own process
group and bounded cleanup applies only to that process group.

## Writable image backup

The acceptance suite's disposable images are for tests and are not flight
workspaces. Before development or flight, use the persistence owner's
documented `rem-sync`/shutdown procedure, then copy the actual writable image
to a second storage location. For example, on a Linux host with the image
available and REM fully shut down:

```sh
cp --sparse=always /path/to/rem-writable.ext4 /path/to/backup/rem-writable-YYYYMMDD-HHMM.ext4
sha256sum /path/to/rem-writable.ext4 /path/to/backup/rem-writable-YYYYMMDD-HHMM.ext4
```

Keep at least one backup separate from the device. Do not copy a writable image
while its guest is running. Follow the current persistence package's image
path and clean-shutdown instructions; this runner does not redefine them.

## Current host limits

This is host-side emulation using the pinned x86-64 QEMU executable; it does
not prove Android/Termux execution or physical Fold behavior. Yash,
Samurai, and networking remain explicitly pending until accepted artifacts
and network hooks are present in the selected tuple. Kilo is reported as not
installed if absent from that tuple.
