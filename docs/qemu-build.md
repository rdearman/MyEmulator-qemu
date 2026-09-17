# Building the QEMU target

This repository is an overlay, not a complete QEMU checkout. The repeatable
workflow uses a full upstream QEMU source tree beside this repository and an
out-of-tree build directory. The defaults are:

```text
source: /path/to/MyEmulator-qemu/.qemu-upstream
build:  /path/to/MyEmulator-qemu/.qemu-build
```

The source checkout is cloned from the QEMU project at `v9.2.0` by
`qemu/tools/build-myemulator.sh`. The script copies the current overlay,
registers the target architecture and its 24-bit internal TCG address-space
parameters; the machine itself still exposes a 16-bit, 64 KiB address space,
configures only `myemulator-softmmu`, and builds
`qemu-system-myemulator`.

The configuration disables QEMU's optional D-Bus display backend because the
CPU tests run headlessly with `-nographic` and do not need display integration.

From the repository root, run:

```sh
qemu/tools/build-myemulator.sh --test
```

The repository also provides a Makefile. The usual commands are:

```sh
make build   # clone/configure/build qemu-system-myemulator
make test    # run the complete project test set
```

The resulting binary is:

```text
.qemu-build/qemu-system-myemulator
```

Useful focused targets are `make cpu-test`, `make debug-test`,
`make device-test`, `make firmware-test`, `make assembler-test`, and
`make emacs-test`. Use `make help` to list them. `make clean` removes only the
generated `.qemu-build` directory; it deliberately retains `.qemu-upstream`.

The test runner is given the newly built binary explicitly, so an unrelated
system QEMU is never used. To rebuild in different locations:

```sh
QEMU_SOURCE=/path/to/qemu \
QEMU_BUILD=/path/to/qemu-build-myemulator \
qemu/tools/build-myemulator.sh --test
```

The same overrides work with Make:

```sh
make QEMU_SOURCE=/path/to/qemu QEMU_BUILD=/path/to/qemu-build-myemulator build
```

`QEMU_SOURCE` may point at an existing full checkout. The overlay application
is idempotent and can be repeated after source or overlay changes. The helper
does not update or reset an existing QEMU checkout; update that checkout
deliberately, then rerun the helper. Set `QEMU_REF` when creating a new
checkout at a different tag or branch.

The first build requires the normal QEMU build prerequisites, including a C
compiler, Python 3, Meson/Ninja or Make’s generated build tooling, and `glib-2.0`
development headers. The CPU test runner additionally requires `xxd`, `rg`,
and `socat`.
