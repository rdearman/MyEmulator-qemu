# Native REM binutils qualification

Run the focused, network-free guest suite from the repository root:

```sh
python3 toolchain/scripts/test-native-binutils-flight.py \
  --output-dir /tmp/rem-native-binutils-qualification
```

The runner refuses to overwrite its output directory. It copies the pinned
flight rootfs to a disposable sparse image, stages one hash-checked native
tool tuple and these fixtures with `debugfs`, boots REM, runs the guest tests,
syncs and restarts the same disposable image, then records `result.json`,
guest output, and console logs. It does not change the flight image. The
QEMU, kernel, and rootfs defaults come from
`docs/REM_VALIDATED_RUNTIME_TUPLES.md`; use `--help` for tuple overrides.

This is focused regression material for the larger flight harness. A
`BINUTILS_QUALIFICATION:PASS` result requires every guest phase, inspection
assertion, and staged core-tool hash check to pass. The live run on
2026-09-27 did **not** pass: native `ar` and `ranlib` crash during usable
archive operations, so library build/link/rebuild and archive persistence
cannot qualify. Single- and multiple-object assembly/link/run, object and
executable inspection, and representative error diagnostics did pass. The
exact reproduction and artifact tuple are recorded in
`docs/REM_NATIVE_BINUTILS_QUALIFICATION.md`.
