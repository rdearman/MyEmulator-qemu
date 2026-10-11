# REM R4 native GNU Make qualification handoff

Status: disposable functional qualification performed; do not replace the R4
installed `/usr/bin/make` until this handoff's artifact is copied and reviewed.

## Environment and provenance

- Tested base: `/tmp/REM-FLIGHT-R4-ANDROID-20261002/rootfs.ext4`, SHA-256
  `3420b0e67bd252c985839b29a7b72d3668f21b50e3aef870a25e858a0cbc946f`.
  All test runs used sparse disposable clones under
  `/tmp/rem-r4-make-fix-work/test*`; the package rootfs was not changed.
- Existing guest binary: GNU Make 4.4.1, `/usr/bin/make`, SHA-256
  `f474110061701a845f11bee32fe09a167b8334fb088940ad33f64452a4630527`.
- Replacement source: GNU Make 4.4.1, built with
  `.toolchain-install/bin/myemulator2-elf-gcc` via
  `/tmp/rem-r4-make-fix-work/rem-gcc-wrapper`, against the existing REM musl
  sysroot and native linker script. Configure used `--disable-posix-spawn`.
- Candidate binary:
  `/tmp/rem-r4-make-fix-work/qualified/make` (same as build output).
  SHA-256 `f5a66f2576603723c1745af4b485a9c3d44af9e020ca59640f4a2ae6b1100f79`.
  Architecture: static MyEmulator2 ELF32.

## Findings

1. `$(wildcard src/*.s)` was empty on the installed Make even though the
   guest directory contained `a.s`, `b.s`, and `c.s`. Configure had selected
   Make's bundled GNU glob implementation (`system libc has working GNU glob:
   no`). Instrumentation showed `glob()` received the correct `src/*.s` input,
   but its directory pointer, formed with `__alloca`, was corrupted before
   directory enumeration. The replacement keeps that pathname in heap storage
   through enumeration and prefixing.
2. The old Make reported `No rule to make target 'build/c.o'` for a present
   `src/c.s` and `build/%.o: src/%.s`. The replacement's dry-run and actual
   build find the rule and bind the implicit variables correctly. The
   replacement moves `pattern_search`'s large dependency/stem buffers from the
   stack to allocated storage and rejects a NULL filename in an inconsistent
   archive-member search safely.
3. The first replacement configure enabled `USE_POSIX_SPAWN` because the
   cross build detected the API, but guest recipe execution returned
   `ENOSYS` (`Function not implemented`). The guest can directly execute
   `/bin/sh` and `/bin/bash`; the failure was Make's selected spawn path.
   Reconfiguring with `--disable-posix-spawn` selected its vfork execution
   path, and actual recipes returned status 0 on the disposable R4 guest.
4. Make's optional load-limit path also entered floating-point runtime code on
   this guest despite the default being no load limit. The REM build tracks
   explicit `-l` use with an integer flag and skips load-limit evaluation
   unless requested. `-l` load limiting is not qualified on REM.

## Source changes in candidate tree

- `lib/glob.c`: allocate the split directory pathname on heap and release it
  after the glob helper has consumed it.
- `src/implicit.c`: heap-allocate the two large search buffers; return no
  match for a NULL archive-member filename.
- `src/main.c`, `src/job.c`, `src/makeint.h`: gate optional load-average
  enforcement on explicit `-l` use.
- Configure: `--disable-posix-spawn`, because its guest implementation returns
  ENOSYS. This is a REM build configuration, not a change to R4's kernel or
  libc.

## Disposable guest qualification

Final run log and tracked QEMU command/PID:
`/tmp/rem-r4-make-fix-work/test16/final.log` and
`/tmp/rem-r4-make-fix-work/test16/qemu-command.json`.

- Clean pre-test clone passed `e2fsck -fn` before boot.
- Test A: `$(wildcard src/*.s)` printed `src/a.s src/b.s src/c.s`.
- Test B: `build/%.o: src/%.s` built `build/c.o`; exit status 0.
- Test C: output showed `target=build/c.o`, `first=src/c.s`, `stem=c`,
  `all=src/c.s`; aggregate output showed correct `$@`, `$<`, and `$^`.
- Test D: second Make reported `Nothing to be done for 'all'`; touching
  `src/a.s` caused `a.o` and the linked aggregate recipe to run.
- Test E: phony target ran twice and wrote two `ran` lines.
- Test F: aggregate target received all three object prerequisites in order.
- Test G: scalable wildcard/patsubst/pattern-rule fixture passed. Rick's
  original REMEDIT Makefile was not available in this workspace, so that exact
  file has not been run here.
- Guest `/bin/sh -c 'echo SH_OK'` and `/bin/bash -c 'echo BASH_OK'` both
  returned 0. The replacement's recipe execution returned 0 after disabling
  posix_spawn.
- The test runner requested `sync2; poweroff`, but QEMU did not exit within 90 seconds. The runner then terminated only its tracked disposable QEMU. Post-test `e2fsck -fn` on that clone reported free-block and free-inode count mismatches (superblock 117481/59786, recount 117136/59770); no repair was performed. This is not a Make failure and no persistence claim is made. The original package rootfs hash remained unchanged.

## Fold Vim comparison: evidence and limits

The R4 rootfs image was inspected read-only. `/usr/bin/vi` is a BusyBox
symlink. `/usr/bin/vim` is a 55-byte shell wrapper which executes
`/usr/bin/vim.real -c 'set nofsync'`; `vim.real` is the 3,163,460-byte Vim
binary. `/etc/inittab` launches getty with `ansi`, and the image has no
`/usr/share/terminfo` tree. This only establishes packaged configuration.
It does not reproduce or identify the Fold-only interactive difference.
BusyBox vi working on the same Fold tty is a strong control, but no Fold
termios/ioctl/input trace was available from this host session. Do not change
TERM, Vim, TTY, kernel, or the installed R4 image on this evidence alone.
A physical Fold capture of `TERM`, `stty -a`, `stty size`, Vim terminal type,
and ioctl/read behavior is still required to assign the fault layer.


## Fold candidate test (no replacement)

- Binary: `/tmp/rem-r4-make-fix-work/qualified/make.new`
- Source patch: `/tmp/rem-r4-make-fix-work/qualified/make-r4-rem-fixes.patch`
- Manifest: `/tmp/rem-r4-make-fix-work/qualified/manifest.json`
- SHA sidecar: `/tmp/rem-r4-make-fix-work/qualified/make.sha256`

The candidate is named `make.new` in the REMEDIT project directory and does
not replace `/usr/bin/make`. Copy the host artifact to a readable Termux path
on the Fold first (for example `$HOME/storage/downloads/make`). The R4
launcher forwards host port 2304 to guest port 2304. Then:

```sh
# In the REM guest, cd to the REMEDIT directory containing the original
# scalable Makefile, then start its receive listener:
rem-receive ./make.new 2304
# In Termux, after the listener starts:
nc 127.0.0.1 2304 < "$HOME/storage/downloads/make"
```

Back in REM, make the candidate executable and run the original project file:

```sh
chmod 755 ./make.new
./make.new list
./make.new clean
./make.new
./make.new test
```

The project directory must contain Rick's original scalable Makefile and
`src/` and `test/` trees. The disposable fixture results are in
`/tmp/rem-r4-make-fix-work/test16/final.log`; this is not a substitute for
the Fold run. `rem-receive` refuses to overwrite an existing destination.
The existing installed R4 `/usr/bin/make` and rootfs remain unchanged.
