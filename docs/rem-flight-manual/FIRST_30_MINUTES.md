# First 30 minutes with REM

This guide is for a desktop UAT session with the disposable Yash-capable REM
tuple. It describes the workflow that has been exercised and marks the parts
that still need final image integration. The tested shell/editor handoff used
a disposable copy of the candidate rootfs; it did not change the flight image.

## 0–5 minutes: boot and reach a shell

Boot the supplied REM desktop image and wait for its login or recovery-shell
prompt. Confirm that typed input is accepted:

```sh
echo REM_READY
pwd
```

Start Yash when it is installed:

```sh
/usr/bin/yash
```

The recovery shell is BusyBox `ash`; Yash is the intended interactive shell.
The tested tuple used `PATH=/bin:/usr/bin:/sbin:/usr/sbin`, `HOME=/root`, and
`TERM=xterm`.

## 5–10 minutes: check the workspace and tools

Use `/root` for work intended to persist in the root filesystem and `/tmp`
for scratch files. Check what is present in this particular image:

```sh
cd /root
pwd
ls /usr/bin/yash /usr/bin/kilo
ls /root/native/bin
```

The candidate rootfs contains Yash, Kilo, chibicc, native `as` and `ld`, CRT
objects, musl `libc.a`, and REM `libgcc.a`. Tool presence does not by itself
mean the full build chain has passed from Yash; see “Build status” below.

## 10–20 minutes: edit a small source file

Open a new file in Kilo:

```sh
kilo /root/hello.c
```

Enter this program:

```c
#include <stdio.h>

int main(void)
{
    puts("Hello from REM");
    return 0;
}
```

Press `Ctrl-S` to save, then `Ctrl-Q` to quit. Kilo was launched from Yash,
saved a C source file, returned to Yash, and the source was read back from
the shell successfully on a disposable copy of the Yash tuple.

## 20–25 minutes: check the saved file

Back at the Yash prompt, inspect the saved text and reopen it:

```sh
cat /root/hello.c
kilo /root/hello.c
```

Quit with `Ctrl-Q` after checking the contents. The verified flow is:

```text
Yash -> Kilo -> save source -> return to Yash -> read saved source
```

## 25–30 minutes: build and persistence gate

The candidate image has the compiler, assembler, linker, and runtime files,
but the end-to-end Yash-driven native compile/link/execute path is not yet
qualified. A previous attempt reported the chibicc child killed by
`SIGHUP`, with no executable produced. Do not treat these commands as a
verified build until the integrated tuple passes them:

```sh
chibicc -o /root/hello /root/hello.c
/root/hello
```

Samurai (`samu`) also requires a current validated binary and successful
integration before a build-file workflow can be demonstrated. For now, use
the saved source as the UAT result and record native building as blocked.

`sync` flushes pending filesystem writes:

```sh
sync
```

Do not power off or reboot during UAT until the desktop image’s clean shutdown
procedure is confirmed. A complete edit/build/run/reboot/reopen cycle has not
passed on this candidate tuple.

## Current usability inventory

| Area | Current evidence |
|---|---|
| Startup | Yash is started explicitly with `/usr/bin/yash`. No Yash startup script has been confirmed in the candidate image. |
| Environment | The tested baseline is `PATH=/bin:/usr/bin:/sbin:/usr/sbin`, `HOME=/root`, `TERM=xterm`. Add `/root/native/bin` to `PATH` only when native tools are integrated and qualified. |
| Aliases | No REM-specific aliases are confirmed. |
| Welcome message | No UAT welcome message is confirmed. |
| Workspace | `/root` is the working location; `/tmp` is scratch space. A standard project directory is not confirmed to exist. |
| Examples | This manual includes a small C example. A preinstalled example-project directory is not confirmed. |
| Editor | Kilo launch, save, shell return, and source readback passed on a disposable copy of the tuple. |
| Build | Tool files are present; Yash-driven chibicc compile/link/execute is blocked by the reported child `SIGHUP`. |
| Build executor | A validated integrated Samurai binary/build has not been confirmed. |
| Reboot persistence | The complete developer-workflow reboot test remains unverified for this tuple. |

## UAT notes to collect

Record the image/kernel/QEMU hashes used, whether a welcome prompt appears,
the result of each editor/build step, and whether the documented clean
shutdown procedure returns to a bootable image. Keep test source under
`/root`; do not modify the flight candidate while using a disposable UAT
image.
