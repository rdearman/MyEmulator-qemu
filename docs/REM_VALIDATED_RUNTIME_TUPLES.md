# Validated REM runtime tuples

These records prevent linker and userspace tests from mixing artifacts from
different kernel, QEMU, or rootfs generations. Hashes are SHA-256. The tuple
below was used for the native chibicc Level 5 test on 26 September 2026.

## Native chibicc hosted-C tuple

| Component | Artifact | SHA-256 |
|---|---|---|
| QEMU | `.rem-phone-deploy-20260925/REM-FLIGHT/qemu/qemu-system-myemulator32` | `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` |
| Kernel | `.rem-phone-deploy-20260925/REM-FLIGHT/boot/vmlinux` | `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` |
| Rootfs | `.rem-phone-deploy-20260925/REM-FLIGHT/disk/rem-rootfs.ext4` | `46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd` |
| Native chibicc | `/tmp/chibicc-rem-stage/chibicc` | `7b9134caaf776953d82d34c3f54dec0eb5b3f343d8ec92ff350df6ee82023a75` |
| Native assembler | rootfs `/root/native/bin/as` | `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93` |
| Native linker | rootfs `/root/native/bin/ld` | `edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65` |
| `crt1.o` | rootfs `/usr/lib/crt1.o` | `9951b664ec350b8ae34c30be9824a42b3a76253978515d674bf1d33b346a4697` |
| `crti.o` | rootfs `/usr/lib/crti.o` | `7fb830f63e7ba115f433b6f28253e8510bbc7b0ab0708c62a2a0ac94aeba5f37` |
| `crtn.o` | rootfs `/usr/lib/crtn.o` | `5594cc2bd0fa13d22e159c78c1f3065b7f36ae217badcdeaf8e71b5c3ad64937` |
| musl `libc.a` | rootfs `/usr/lib/libc.a` | `30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924` |
| REM `libgcc.a` | staged GCC runtime added only to disposable test rootfs | `759fb011a7d4dfba4da8a96d17cc978485f2daf62c2ba4545180d4de40773044` |

The base rootfs does not currently contain `libgcc.a`. It is required for a
hosted C link because musl's `libc.a` references compiler helpers such as
`__muldi3`. The Level 5 test used the archive above in a disposable rootfs;
the permanent flight image was not modified.

## Verified chain

With the tuple above, the following stages completed in the disposable guest:

```text
/root/chibicc -S -o /root/hello.s /root/hello.c       -> 0
/root/native/bin/as -o /root/hello.o /root/hello.s    -> 0
/root/native/bin/ld ... crt + libc + libgcc ...       -> 0
/root/hello                                             -> 0
```

The program printed `Hello from REM` and returned status zero. Keep the QEMU,
kernel, rootfs, binutils, startup objects, libc, and compiler-runtime archive
together when repeating this test.
