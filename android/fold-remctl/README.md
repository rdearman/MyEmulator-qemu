# Fold REM launcher source

`remctl` and `rem` are the canonical launcher sources reconciled from the
physical Fold capture at `android/r4/fold-active-20261011/`. The captured
files and accompanying text are retained unchanged as deployment evidence.
The canonical `remctl` preserves that launcher's active diagnostic QEMU path,
512 MiB RAM, user/slirp host forwarding, block device, socket console, stale
state inspection/recovery rules, and fail-closed QEMU integrity checks. It
contains no `-icount` argument.

The current device's QEMU hashes are deployment data, not permanent launcher
constants. Before installing a release, generate
`$HOME/rem/.remctl/qemu-sha256` from the exact base and active QEMU artifacts:

```sh
bash write-qemu-sha256.sh \
  "$HOME/rem/qemu-system-myemulator32" \
  "$HOME/rem/diag/qemu-system-myemulator32.r15-diag" \
  "$HOME/rem/.remctl/qemu-sha256"
```

The generated file contains exactly two records, `base <sha256>` and
`active <sha256>`. `remctl` parses it as data, rejects missing, duplicate,
malformed, or unknown records, and keeps the previous hash-gated launch and
managed-process recognition behavior. The hash writer uses a temporary file
and rename so an interrupted write cannot leave a partial accepted file.

Install `remctl` at `$HOME/rem/.remctl/remctl`, install `rem` at the Termux
`$PREFIX/bin/rem` path, and install `rem-console-client` alongside `remctl`.
The physical Fold's active launcher paths are authoritative; the older
`android/launch-rem*.sh` scripts remain separate desktop/flight launchers.
