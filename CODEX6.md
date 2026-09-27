# Codex #6 handover — REM Emacs startup fault isolation

**Assignment:** isolate why corrected-toolchain GNU Emacs 30.1 does not start in REM.

**Status:** the first reproducible startup blocker is isolated to blocking entropy acquisition in `init_random()`. Emacs remains unqualified; an earlier default-stack fault is still unresolved.

## Findings

The corrected Emacs ELF is a static REM ELF32 with a valid `_start`, linked using current repository GCC, REM `as`/`ld`, musl CRT/libraries, and the REM linker script. Its SHA-256 is:

```
bb4d31623a70a3e314de017791202b8ef0610202c41f4ac6c33c121f627022d6
```

Temporary instrumentation of `main` showed successful return from argument sorting, `getcwd`, stack-limit handling, `setlocale`, and `init_signals`. Later markers reached `init_threads()` and then `init_random()`; no following `init_xfaces()` marker appeared. The traced command was `emacs --version`, which emits its version only after significant core initialization. The instrumentation binary SHA-256 was:

```
51df22a816b39d285b25456c4e7f7ed13fde4b3fe2655a6a5bedb4cb822d4fd0
```

Emacs 30.1 `src/sysdep.c:init_random()` calls:

```c
getrandom(&v, sizeof v, 0)
```

A standalone native REM probe using flags `0` printed `GETRANDOM_BEFORE`, remained alive after three seconds, and had to be killed. A probe using `GRND_NONBLOCK` printed `GETRANDOM_NOT_READY` from its explicit `EAGAIN` branch. Its source is [`toolchain/tests/emacs-stack/getrandom-early-boot-probe.c`](toolchain/tests/emacs-stack/getrandom-early-boot-probe.c), SHA-256 `5e4255aebd5ad43cf46b3de8bdc6492c2fa0256136ee2166fe05df18f6a7e85b`; its native executable SHA-256 is `a2c48db59a58f17470cdfeefd91fd82c9abb82b0a16b3c1bc9d7c111ab73c701`.

The kernel dispatches `getrandom` as syscall 278 and musl declares the same number. The nonblocking probe demonstrates that it is implemented and reports the random source as not ready. Flags-zero `getrandom()` blocking until the urandom source initializes is Linux behavior ([`getrandom(2)`](https://man7.org/linux/man-pages/man2/getrandom.2.html)); Emacs is waiting according to that contract rather than failing on an absent syscall.

The inspected kernel build configuration has `CONFIG_HW_RANDOM` and `CONFIG_CRYPTO_JITTERENTROPY` disabled. The current QEMU machine source creates console, block, and optional virtio-net devices but no RNG device. The inspected `.config` has not been tied to the exact deployed kernel hash, so this suggests—but does not prove—the tested guest lacks an entropy producer. The guest random-pool initialization path remains unverified.

## Separate stack fault

An earlier uninstrumented run of the corrected Emacs binary ended in `__malloc_alloc_meta` with a write fault at SP `0x3f7d6fe4`, 8,388,056 bytes below the initial stack pointer `0x3ffd6dbc`. Its kernel diagnostic reported no VMA after stack expansion failed. Small malloc/free and large-stack probes passed under the current GCC, including 9 MiB alloca with a 16 MiB stack limit.

The current trace stops at the entropy wait and did not reproduce that stack-fault frame. It is not yet known whether the stack fault is independent or appears after entropy becomes ready. Do not treat it as disproven.

## Test environment and constraints

The earlier fault test used kernel SHA-256:

```
c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63
```

and QEMU SHA-256:

```
0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a
```

Instrumentation and executable probes were placed only in `/tmp` and disposable ext4 images. No kernel, libc, compiler, or production-image code was changed. The disposable image used for probes should not be treated as a production artifact.

## Documentation

- [`docs/REM_EMACS_QUALIFICATION.md`](docs/REM_EMACS_QUALIFICATION.md) records rebuild provenance, stack evidence, instrumentation boundary, and entropy probes.
- [`docs/REM_ARCHITECTURE64.md`](docs/REM_ARCHITECTURE64.md) records the ABI/runtime and entropy-source implications, with required Linux behavior distinguished from recommended REM design and open work.

## Next steps

1. Have the platform boot/device integration owner provide an entropy source or an initialized boot seed before starting userspace. No RNG owner was named in the current worker roster; Codex #4 owns flight-image integration, while the kernel/QEMU mechanism needs an explicit owner.
2. Rerun `getrandom-early-boot-probe` with both flags-zero and `GRND_NONBLOCK`; record whether the pool is ready before Emacs starts.
3. Rerun corrected Emacs `--version`. If it proceeds past `init_random()`, capture further low-overhead markers and check whether the earlier stack fault recurs.
4. Only after startup is established, attempt `--batch`, `-nw`, edit/save, and Yash readback. None has passed.

## Current acceptance

- Corrected-toolchain ELF/startup: **PASS**
- Early argument, cwd, stack-limit, locale, signal setup: **PASS**
- Native REM `getrandom` syscall present: **PASS**
- Random pool ready at userspace launch: **FAIL** (`GRND_NONBLOCK` reports not ready)
- `emacs --version`: **FAIL / BLOCKED at blocking entropy request**
- Default-stack fault fully explained: **OPEN**
- `--batch`, terminal editing, save, and readback: **NOT TESTED / NOT ACCEPTED**
