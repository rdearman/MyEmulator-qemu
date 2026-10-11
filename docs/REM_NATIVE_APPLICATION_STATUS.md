# REM Native Application Bootstrap Status

Validated with the REM chibicc backend (source tree at commit
`395ec4f61bb4d8557e1a5d7f26bb960fea48d0bf`), native REM `as`, native REM `ld`,
the pinned musl CRT/libc and libgcc tuple.

## Kilo

- Source: bundled canonical Kilo 0.0.1 snapshot,
  `tools/rem-flight-image/staging/applications/kilo-source/kilo.c`.
- Compiler: current `toolchain/chibicc-rem/chibicc`, SHA-256
  `dfc8aa0a9f4058ae9d8fb5b2345f15f6bf7601c891edee99a379ab07dd1054f7`.
- Result: preprocessing, compilation, assembly and static linking PASS.
- Binary: `/tmp/kilo-rem-build/kilo`, SHA-256
  `007084b2fa9564951a55c4538bb4f50137c9ce8f06ab2c6fdc66c14f64ce87ac`.
- Runtime: startup reached terminal initialization. `TIOCGWINSZ` returned zero
  rows/columns in the disposable serial environment, so Kilo entered its
  cursor-position query fallback and waited for a terminal response.
- Classification: terminal geometry/capability boundary; no compiler failure
  remains for this source.

## Small utilities

| Program | Source | SHA-256 | Result |
|---|---|---|---|
| `linegrep` | `toolchain/examples/flight-project-pack/linegrep/linegrep.c` | `0c437851e5d212196f0f7663ae8292c823fe6b84c9a0503ad2aabc57977bdf88` | Built and ran; matched `ERROR` line |
| `bitcount` | portable C implementation in `bitcount/` | `5899ae0e10129d61add8a007ec123e5cb8eab9b2c813c5bb4ac877affdca0e89` | Built and ran; `bytes=2 set-bits=4` |
| `memo` | `memo/main.c` plus `note_store.c` | `237084b2736329cec6b0b143b7dbb856f60c1d51c7f2cc1ee9907e0378833157` | Built; add passed, list/find faulted with alignment bus error |

The `memo` failure is not yet classified as compiler or runtime; it requires a
GCC-built comparison and minimal reducer before any compiler change.

## Lua and SQLite

Neither upstream source tree is present in the repository or disposable
workspace. Network DNS was unavailable when attempting to obtain Lua and
SQLite, so no build result is claimed. Their first executable test remains
pending source staging.

## Chibicc self-hosting

A current native REM chibicc executable was staged into a clean guest together
with the complete compiler source. The first source-level blocker,
`__builtin_ctz` in `codegen.c`, was removed in favour of an ordinary C helper.
The host-generated `codegen.c` assembly then assembled successfully. The
native guest attempt still did not finish compiling that large translation unit
within the observed timeout, so Level 7 self-hosting is **OPEN** and no
self-hosted compiler artifact is claimed.
