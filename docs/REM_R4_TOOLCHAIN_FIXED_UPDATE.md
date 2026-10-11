# R4 corrected chibicc/Samurai update staging

This independent update is staged outside the R4 rootfs and does not alter the
Fold installation. It follows the corrected REM32 ILP32 compiler work recorded
in `docs/CODEX-2-HANDOFF.md`.

Artifacts copied without rebuilding:

* `chibicc` from `/tmp/chibicc-fix-build/chibicc`, SHA-256
  `9aa9782dfeeac64e2ffa22d222f88afb3e2293cd19dc34d857bbf721a05ac761`.
  The source corrections use `int64_t` for preprocessor constant evaluation
  and select `unsigned long long` for ILP32 suffixed 64-bit hexadecimal
  constants. The focused `rem-pp-uintptr-width.c` regression passed.
* `samu` from `/tmp/samu-fix`, SHA-256
  `1bee2e3c9aa19fe0ece0af1eb333b41e63ba7382bff993b68d297515d9d1aa93`.
  The qualified source set uses upstream `NO_POSIX_SPAWN`; only `log.c` needed
  rebuilding after the compiler correction. Reported disposable guest tests:
  real `touch hello`, no-op rebuild, `samu -n`, and changed-input rebuild all
  returned zero.

The frozen earlier compiler `/tmp/chibicc-stage7` remains untouched at hash
`72bf5c9b519eb2a33ab5eb2e014a48d21ef661049e240b3a42000f39f423eba0`. The new
update script installs the corrected pair to `~/.local/bin`, backing up any
existing `chibicc` and `samu` there first. It verifies both hashes before any
copy. It does not install into `/usr/bin` or touch system/rootfs files.

Package: `/tmp/REM-R4-CHIBICC-SAMURAI-FIXED-20261004.tar.gz`.
The bundle is an optional deployment artifact; it has not been run on the
Fold. Do not install it while Fold/Termux stability is being recovered.
