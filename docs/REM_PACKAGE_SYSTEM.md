# REM package system design and `rempkg`

## State and scope

`tools/rempkg/rempkg` is a first local prototype for offline, single-system
packages. It uses Python 3's standard library and `.rmpkg` gzip-compressed tar
archives. The inspected development rootfs contains Python 3.12, but guest
execution of this prototype has not been qualified. It is not a package
repository client, dependency solver, signature verifier, or replacement for
the image builder.

## Package format

A package source directory contains:

```text
PACKAGE/
  package.json
  payload/                  # paths relative to the guest root
```

`package.json` has one-line `name`, `version`, `arch`, `description`, and
`source` fields, a `dependencies` list, and a `build_tuple` object. A tuple
has an `id` and `kind` (`native`, `script`, or `data`). Native tuples also
contain the ABI, build-manifest SHA-256, and SHA-256/name/version records for
the compiler, libc, and binutils. `rempkg pack` detects MyEmulator2 ELF
executables and refuses to package one without a complete native tuple.
Script-only or architecture-independent packages identify their kind and
tuple ID. `rempkg pack PACKAGE OUTPUT.rmpkg` writes a `rempkg.json` index with
SHA-256 hashes, types, modes, and symlink targets plus the `payload/` tree.
Regular files, directories and safe relative symlinks are supported. Special
files and hard links are rejected. Paths cannot be absolute, contain `..`,
or descend through a package symlink.

The database is stored under `/var/lib/rempkg/NAME.json`. Packages may not
overwrite existing paths. Removal checks that each owned file still matches
its recorded hash/target and refuses to delete modified or missing files.
Empty package-created directories are removed; shared/nonempty directories
remain. Installed JSON records retain the source, build tuple, and dependency
list; `info` displays them and `list` shows the tuple. Native package install
requires `/etc/rem-release` and an exact matching `Build tuple:` ID. The
manifest hash identifies the separately retained tuple manifest; rempkg does
not fetch or authenticate that manifest. Dependencies are recorded but are
not resolved or installed automatically. `verify` checks installed files.

## Commands

```text
rempkg pack DIRECTORY ARCHIVE.rmpkg
rempkg install [--root ROOT] ARCHIVE.rmpkg
rempkg remove [--root ROOT] NAME
rempkg list [--root ROOT]
rempkg info [--root ROOT] NAME_OR_ARCHIVE
rempkg verify [--root ROOT] [NAME]
```

Use `--root` only for a disposable root tree, qualification, or image staging.
For a live root filesystem, install/remove requires root privileges. Review
packages before installation. The manifest detects accidental corruption,
not a malicious packager: packages are unauthenticated and must come from a
trusted offline source.

## Qualification

Host lifecycle tests are in `tests/rempkg/test_rempkg.py`:

```sh
python3 -m unittest discover -s tests/rempkg -v
```

They cover pack/install/list/info/verify/remove, modified-file detection,
overwrite refusal, and an escaping-symlink rejection. This does not qualify
the Python runtime, permissions, atomicity across power loss, or package
installation on REM. Before guest integration, install the script under
`/usr/bin/rempkg`, test a disposable root with real REM Python, and verify a
clean shutdown/reboot. Keep packages local; no network feature is required.

## Deliberate limits and next design steps

There are no dependency scripts, pre/post-install hooks, upgrades, triggers,
alternatives, signatures, repository index, conffile merge, or automatic
dependency resolution. No package should install over files owned by another
package. Before flight/system use, add a package format version policy, an
offline signing/trust decision, crash recovery/journal strategy, path and
permission review, and actual guest qualification. Continue to use the image
builder for kernel, init, rootfs layout, and matched compiler/runtime tuples.
