# REM userspace regression harness

This harness tests the actual REM ELF applications in a disposable ext4
copy. It is deliberately separate from application repairs: a crash remains
a crash in the report and does not get converted into a pass.

Inventory an archive or extracted tree:

```sh
python3 toolchain/userspace/inventory.py \
  --archive imports/userspace/rem-userspace-05c9a0f69845.tar.gz \
  --output /tmp/rem-userspace-inventory.json
```

The host runner is:

```sh
python3 toolchain/scripts/test-userspace.py \
  --rootfs /tmp/rem-userspace-rootfs-shell.ext4 \
  --output /tmp/rem-userspace-regression
```

It copies the image, injects `guest-regression.sh` using `debugfs`, boots the
configured REM QEMU and kernel, captures the complete console, and writes
`report.json` and `summary.txt`. Use `--test coreutils-ls`, `--group python`,
or `--qemu-trace` for focused diagnostics. The runner always terminates the
QEMU process it created; it never searches for or kills unrelated QEMU
processes.

The initial suite is intentionally conservative. Tests that require network
access are startup-only or omitted until networking is enabled. Add a test by
adding a `run_test` invocation to the guest script and documenting whether it
is functional, startup-only, or not tested.
