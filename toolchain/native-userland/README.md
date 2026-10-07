# REM native-userland source kit

This directory holds everything REM-specific that is needed to recreate the
transferable native-userland source kit: the compiler/runtime bootstrap, the
patched package sources with their Samurai build plans, the regression tests,
the documentation and the build, install and rollback scripts. It does not
contain upstream source code. The upstream releases are listed with their
URLs and SHA256 sums in `sources.env` and are fetched or verified when the
kit is assembled.

The repaired compiler is kept in `toolchain/chibicc-rem` (nested repository).
`patches/chibicc-rem-native.patch` is the same source expressed as a patch
against upstream chibicc `90d1f7f199cc55b13c7fdb5839d1409806633fdb`.

## Layout

| Path | Contents |
| --- | --- |
| `sources.env` | Upstream archive, URL, SHA256 and kit destination |
| `assemble-kit.sh` | Recreates the kit tree under `/tmp` |
| `patches/` | Complete REM deltas against pristine upstream (`patch -p1`) |
| `plans/<package>/` | Samurai `build.ninja.in` plans and generated configuration headers, copied to `packages/<package>/rem-native-plan` |
| `executable-files.list` | Kit files that need the execute bit (patches do not carry modes) |
| `kit/` | Top-level kit files: `build-all-native.sh`, `install-native.sh`, `rollback-native.sh`, `acceptance-native.sh`, `native-env.sh`, `README-REM.txt`, `certificates/`, bootstrap scripts |
| `host-tools/` | Host-side plan generation and source-emission checks |

The kit's `tests/` directory comes from `toolchain/tests/native-userland/`.
Its `docs/` directory comes from `docs/CHIBICC_UPSTREAM_REPORT.md` and
`docs/REM_CHIBICC_BUGLOG.md`.

## Assembling the kit

    toolchain/native-userland/assemble-kit.sh /tmp/rem-native-userland-work/kit
    REM_NATIVE_WORK=/tmp/rem-native-userland-work \
        python3 toolchain/native-userland/host-tools/package-source.py \
        /tmp/REM-NATIVE-USERLAND-SOURCE.tar

Archives are downloaded into `$REM_NATIVE_DOWNLOADS` (default
`/tmp/rem-native-userland-downloads`), or used from there if already present.
Every archive is checked against `sources.env` before it is extracted.

The tar archive has a single top-level directory, `rem-native-userland/`.
On REM, extract it and run `./build-all-native.sh` and then
`./install-native.sh`. `kit/README-REM.txt` describes the REM procedure.

## Host checks

`host-tools/check-native-plan.py PACKAGE` compiles every translation unit in
a plan to REM assembly with a host build of the kit compiler. It needs the
following under `$REM_NATIVE_WORK`:

- the compiler at `bootstrap-host-syntax/chibicc-host-current`, built from
  `kit/bootstrap/chibicc` with `-DCHIBICC_REM` and Ryu `s2d.c`;
- the BearSSL and ncurses headers in `host-build-plans/native-deps/include`.

These checks only confirm that the compiler emits code for every source
file. They are not native acceptance: building and running the packages on
REM is still required.

## Provenance

This content was recovered on 2026-10-07 from a standalone working tree
(commit `42de157a`) that had been created outside this repository. Each kit
file was compared byte for byte with a kit assembled from this directory.
Generation paths embedded in generated plan files (ncurses `MK*` outputs,
Dropbear's options guard, Vim's `auto/pathdef.c`) were rewritten to be
kit-relative; they occur only in comments or `:version` text.
