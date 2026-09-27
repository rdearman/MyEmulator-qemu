# REM Flight Checklist

Use this checklist against the exact image, kernel, QEMU, and toolchain tuple intended for the flight. Record the hashes before departure.

## Boot and console

- [ ] Boot reaches `REM#`.
- [ ] Typed commands execute, rather than merely appearing in the serial transcript.
- [ ] `/dev/ttyMY0` is the shell's standard input, output, and error device.
- [ ] `/proc` and `/sys` are mounted when the image expects them.

## Shell and editor

- [ ] Launch `/usr/bin/yash` from the parent shell.
- [ ] Run `echo hello`, `pwd`, and `ls`.
- [ ] Test variables, a pipe, redirection, history, and exit.
- [ ] Launch `kilo /root/projects/flight-test.s`.
- [ ] Insert text, navigate, search, save with `Ctrl-S`, quit with `Ctrl-Q`, and reopen the file.

## Toolchain provenance

- [ ] `PATH` puts `/root/native/bin` before `/usr/bin`.
- [ ] `chibicc`, `as`, `ld`, `ar`, `ranlib`, `objdump`, and `readelf` resolve to the authoritative tuple.
- [ ] `chibicc -o hello hello.c` produces a REM executable.
- [ ] `./hello` produces the expected output.
- [ ] For assembly, `as` creates an object and `ld` links it with the REM linker script.

## Build workflow

- [ ] Build a small multi-file project.
- [ ] Inspect its object and executable with `readelf` and `objdump`.
- [ ] If Samurai is installed, `samu -h` passes.
- [ ] `samu -n` and an actual build remain **PENDING** until explicitly validated in the integrated image.

## Persistence

- [ ] Run `sync` after editing and building.
- [ ] Shut down using the image's clean shutdown procedure.
- [ ] Reboot the same filesystem image.
- [ ] Reopen the edited source and verify its contents.
- [ ] Re-run the compiled program after reboot.
- [ ] Re-run the build or incremental rebuild after reboot.

Persistence is currently **PARTIAL** until the complete edit → assemble/link → execute → clean reboot → reopen → rebuild workload passes.

## Offline readiness

- [ ] All required manuals and examples are present in the offline documentation directory.
- [ ] No step depends on downloading source or packages.
- [ ] Disposable test artifacts are not being mistaken for the production flight tuple.
