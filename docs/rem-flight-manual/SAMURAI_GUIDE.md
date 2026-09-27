# Samurai Guide

Samurai (`samu`) is a Ninja-compatible build executor intended to provide a small, explicit build workflow on REM. It is a candidate replacement for GNU Make, not a different compiler.

**VERIFIED:** `samu -h` starts and prints its usage text in the disposable REM validation environment.

**PENDING:** `samu -n`, real builds, and incremental rebuilds require the complete integrated native toolchain and remain unverified in the flight image.

## Build file

A build file describes targets and their dependencies:

```ninja
rule cc
  command = chibicc -c -o $out $in

rule link
  command = chibicc -o $out $in

build build/main.o: cc src/main.c
build build/app: link build/main.o
```

The default target is the first build statement unless a target is named explicitly:

```sh
samu -f build.ninja
samu -f build.ninja build/app
```

## Dependencies and incremental builds

Samurai compares input and output state and rebuilds targets whose inputs changed. Keep generated files under `build/` and sources under `src/`:

```text
project/
  src/
  build/
  samu.build
```

Use a clean target when the project defines one. **FUTURE:** confirm timestamp behavior and persistence after the final image integration; REM guest clock behavior has affected historical Make tests.
