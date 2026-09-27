# Codex #5 handover

**Assignment:** REM OpenSSH compatibility validation  
**Status:** Partial. The controlled build produced selected REM executables, but the guest runtime tests failed before SSH behavior could be validated. No production artifacts were changed.

## Work completed

- Built Portable OpenSSH 10.5p1 with the current REM GCC/libc/linker inputs and recorded their hashes, configure options, and build results in [docs/REM_OPENSSH_COMPATIBILITY.md](docs/REM_OPENSSH_COMPATIBILITY.md).
- Added `toolchain/scripts/test-openssh-rem.py`. It checks artifact identities and static REM ELF format, validates candidate manifest guest files, tests on a disposable rootfs copy, boots QEMU, and records per-command results and serial output.
- Updated the authorized-key staging test to fail if the generated public key is missing.
- Updated the relevant OpenSSH filesystem observation in [docs/REM_ARCHITECTURE64.md](docs/REM_ARCHITECTURE64.md). The note distinguishes deployment configuration requirements from behavior that the faulting binaries did not reach.

## Reproduced guest result

Rerun command:

```sh
python3 toolchain/scripts/test-openssh-rem.py \
  --candidate-dir /tmp/rem-flight-candidate-20260927-chibicc-final3 \
  --refresh-candidate-rootfs-hash \
  --build-dir /tmp/rem-openssh-20260927/openssh-10.5p1 \
  --report /tmp/rem-openssh-20260927/guest-report-final-rerun.json
```

The result is **FAIL**. Candidate verification, guest boot, and bidirectional serial command/response worked. The candidate rootfs stayed unchanged. The run deliberately refreshed its observed hash after manifest-listed guest files passed verification; this does not accept the manifest-pinned image as a flight candidate.

Exact runtime tuple:

| Artifact | SHA-256 |
|---|---|
| QEMU | `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` |
| Kernel | `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` |
| Candidate rootfs actually tested | `1127493c4abe15fc9bf9993f34c5d8d4b1d23c73fd8c3ff1c4c2282e5d52ae8d` |
| Candidate manifest rootfs hash | `c9f19fab0d27b7a96bec09bf09fc38f37059f7e462d2b9ca4e68bf57fb61c0de` |

OpenSSH output hashes and build input hashes are in the compatibility report and machine-readable guest report at `/tmp/rem-openssh-20260927/guest-report-final-rerun.json`.

Observed failures:

- `ssh -V`: exit 139, `MYEMU_BAD_USER_FAULT` at PC `02000180`, address `00001070` (`ssh` `main+0xac`).
- `ssh-keygen`: exit 139 at PC `020e1688`, address `0000023c`; symbolization points into `evp_default_properties_enable_fips_int`.
- `sshd -t`: exit 139 at PC `0203665c`, address `00000574`; symbolization points to `sshkey_private_to_blob2` with no source line.
- The `stat` key-mode check also faults; it does not establish permission behavior.
- The full OpenSSH `make` fails in upstream `scp.c` because `glob_t.gl_matchc` is unavailable in the selected musl headers.
- `MYEMU llist bad add` appeared during fault handling. Causality is unknown.

Runtime fault classification is unresolved among compiler-generated code, ABI, and libc/runtime. Networking, key permissions, account lookup, daemon startup, signals, socket operations, and PTY allocation were not validated. Do not report these as passes or attribute the fault to a subsystem without further evidence.

## Next steps

1. Give the exact command, ELF hash, tuple hashes, fault PC/address, and serial trace in the compatibility report to the relevant compiler/runtime/kernel owners for attribution. Do not modify their components from this workstream.
2. Once a fixed or otherwise validated OpenSSH executable tuple is available, rerun the OpenSSH harness first. Confirm `ssh -V`, key generation, and `sshd -t` before relying on later daemon/network/PTY results.
3. Run the full suite only after those prerequisites pass. Record each layer separately; a successful boot or shell prompt is not OpenSSH acceptance.
4. The ordinary base tuple rootfs also failed its pinned hash check (`46604d…` expected, `04eaef…` observed). Preserve exact artifact hashes and do not silently refresh tuple identities.

## Files and reports

- `toolchain/scripts/test-openssh-rem.py` — guest harness.
- `docs/REM_OPENSSH_COMPATIBILITY.md` — source provenance, build inputs, output hashes, failure analysis, and rerun command.
- `docs/REM_ARCHITECTURE64.md` — concise account/privilege-separation filesystem observation; runtime behavior remains marked unvalidated.
- `/tmp/rem-openssh-20260927/serial.log` — latest QEMU serial log.
- `/tmp/rem-openssh-20260927/guest-report-final-rerun.json` — latest machine-readable results and artifact identities.

Host-side validation completed: Python syntax check and `git diff --check` passed for the changed harness/documentation. No Fold or other physical hardware execution was performed.
