# REM administration staging

The old `rem-account-base/sudo` shell wrapper called BusyBox `su`, but the
R4 BusyBox binary is deliberately not setuid. That wrapper therefore never
provided privilege elevation. Do not mark the multicall BusyBox executable
setuid: that would make every enabled applet a root entry point.

Build the dedicated REM sudo helper with `./build.sh [output-path]`. It is a
static MyEmulator2 ELF32 binary linked against the REM musl `libcrypt`. The
helper is installed root:root mode 4755 and authorizes UID 1000 (`dev`) after
checking that user's password from `/etc/shadow`. `sudo -i` starts a root Bash
login shell; `sudo COMMAND ...` executes the requested command with a clean
root PATH. The configured policy is intentionally limited to the `dev` account
for this single-user workstation.

For a fresh filesystem tree, run as root:

    toolchain/userspace/rem-admin/install-into-tree.sh ROOTFS-TREE BUILT-SUDO

Then build the ext4 image from that tree while preserving file ownership and
mode (including the setuid bit). Verify the image with `debugfs stat
/usr/bin/sudo` and require uid 0, gid 0, mode 0104755. `/etc/shadow` must be
root:root mode 0600. The staged root password is `remdev`; dev's staged
password is `remtest`. The hashes explicitly use SHA-512 crypt with 1000
rounds. The shipped BusyBox uses its internal SHA-crypt implementation, whose
implicit round count differs from the host `crypt(3)` default. Host-default
hashes consequently failed BusyBox login even though REM libc verified those
same hashes. Explicit-round hashes passed serial login for both accounts;
retain the explicit rounds field when regenerating account templates. The
serial console is listed in `securetty` as `console`, `ttyMY0`, and `UNKNOWN`;
R4 BusyBox reports the serial terminal as `UNKNOWN`, so root console
authentication requires that entry.

For an existing R4 guest, use the accompanying recovery update artifact and
installer only from a root maintenance shell. It sets the documented root
password to `remdev` and dev password to `remtest` using compatible explicit
round counts, and changes `/etc/securetty`, `/etc/sudoers`, and `/usr/bin/sudo`.
It preserves account IDs, homes, other accounts, `/home/dev`, REMEDIT, BusyBox,
and `/usr/bin/make`. It creates backups of the files it changes before writing
them.

The canonical account installer (`tools/userspace/install-rem-account-base.sh`)
builds or installs the dedicated sudo helper root-owned mode 4755 and the
policy file mode 0440. It deliberately leaves BusyBox non-setuid; its
multicall applets are not the privilege boundary.
