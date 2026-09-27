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

Vim is test 27. It is kept separate from the 26 shell-driven tests because
the acceptance case requires serial-terminal input:

The existing `test-userspace.py` runner contains the original 26
non-interactive application tests.  Vim is kept as test 27 because it
requires serial-terminal input rather than a shell command:

```sh
python3 toolchain/scripts/test-vim.py \
  --rootfs /tmp/rem-vim-rootfs.ext4 \
  --qemu .qemu-build/qemu-system-myemulator32 \
  --kernel .linux-build/build/vmlinux \
  --output /tmp/rem-vim-regression
```

The Vim test starts `/usr/bin/vim` in the guest, inserts text into an
assembly file, writes it, exits, and verifies the saved marker.  Its JSON
result and serial transcript are written beneath the selected output
directory.  A nonzero result is a genuine failure, timeout, or guest crash;
it must not be converted to a pass by replacing Vim with a BusyBox applet.
