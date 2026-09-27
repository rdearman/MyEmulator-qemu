# GNU Make qualification for REM

## Result: PASS

GNU Make can run native build recipes inside the disposable REM guest after
replacing the cross-configured binary and guarding Make's invalid archive
filename path. The qualified binary is staged at
`toolchain/native-make-flight-overlay/bin/make` for final image integration.
Its SHA-256 is
`f474110061701a845f11bee32fe09a167b8334fb088940ad33f64452a4630527`.

The test used GNU Make 4.4.1 and a small C + assembly project. Inside REM:

1. `make` ran chibicc (`hello.c` to `hello.s`), native `as` (both objects),
   and native `ld` (the executable).
2. `./hello` returned 0. Its startup assembly calls the C `flight_check()` routine and exits successfully only when that routine returns 42.
3. `make clean` removed the generated files and returned 0.
4. A clean `make` rebuilt and ran successfully.
5. `touch hello.c && make` re-ran chibicc, assembler, linker, then returned 0.

The guest emitted a timestamp-out-of-range warning (reported time
`2514-05-30`) from the tested kernel/time tuple. It did not prevent builds,
clean, touch-triggered rebuild, or execution. The Make build also rebuilt the
assembly startup object during the touch step, so the fixture proves a
successful source-triggered rebuild, not minimal dependency selectivity.

## Root causes and changes

The prior Make configuration enabled `USE_POSIX_SPAWN` after a cross-build
probe. REM's process implementation does not support the spawn clone flags,
so recipes failed with `Function not implemented`. The qualified build
configures the traditional fork/vfork path (`vfork` maps to `fork`) and
excludes Make's optional stack-size reset hook. Its recipes then launched and
ran normally.

The prior binary also faulted in `pattern_search()` at `strlen(NULL)` during
default implicit-rule lookup. The included source patch returns “no rule” when
archive lookup produces a null member filename. With this guard, normal
implicit C and assembly rules were searched and used by the build fixture.

The binary is statically linked. Its integration copy was tested at
`/usr/bin/make` in a writable disposable clone of the REM rootfs. The
underlying flight image was not modified.

## Reproduce in REM

Use the fixture from `toolchain/tests/native-make/` with the native toolchain
paths available in the guest. The essential commands are:

```sh
make
./hello
make clean
make
touch hello.c
make
```

The startup assembly validates that C returned 42 and exits 0; each Make invocation and the program return status 0.

## Integration requirements

Install the overlay binary into the final rootfs as `/usr/bin/make`. If
rebuilding GNU Make 4.4.1, apply the overlay patch and pass
`--disable-posix-spawn`; do not trust the cross-compiled spawn runtime probe.
The qualified binary and test hashes are recorded in
`toolchain/native-make-flight-overlay/README.md`.
