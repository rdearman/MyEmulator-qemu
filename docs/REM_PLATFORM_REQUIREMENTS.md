# REM platform requirements

These requirements apply to every bootable REM Linux image and its image
builder.

## Console contract

The virtual motherboard provides a kernel console and the REM serial terminal
device `/dev/ttyMY0`. A visible prompt is not sufficient evidence that the
console works: interactive input and output must travel through the same tty.

An interactive init shell must attach all three standard descriptors to the
real terminal device:

```sh
TTY=/dev/ttyMY0
exec /bin/bash --login -i <"$TTY" >"$TTY" 2>&1
```

`/dev/console` remains the kernel-console abstraction for early diagnostics and
fallback reporting. It must not be used as the normal interactive shell’s
stdin when it is a regular rootfs file or otherwise lacks tty input semantics.

## Device creation and boot self-test

The kernel configuration enables devtmpfs and devtmpfs auto-mount. Image
builders must also provide a `/dev/ttyMY0` character node in the early image
when the init sequence can run before devtmpfs registration. The REM device is
major 240, minor 0 in the current Linux port.

Before launching an interactive shell, init checks that `/dev/ttyMY0` is a
character device. If it is missing, init prints an explicit error and a device
listing to `/dev/console`, then enters a recovery shell. Image validation must
send a command after the prompt and verify its output; matching only `REM#`
does not prove bidirectional console operation.

The required boot smoke test is:

```text
boot image
wait for prompt
send: echo REM_CONSOLE_TEST
expect: REM_CONSOLE_TEST
```

This checks the complete path from host input through QEMU and the REM tty to
shell stdin, and back through shell stdout, the tty, QEMU, and the host.

## REM64 implication

Console device naming, tty ownership, early device creation, and the
bidirectional boot test are virtual-motherboard requirements. They must be
specified alongside storage and other boot devices so each future image does
not rediscover them independently.
