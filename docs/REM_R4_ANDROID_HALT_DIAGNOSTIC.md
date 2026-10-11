# R4 Fold idle-fault diagnostic QEMU

## Captured fault and symbolisation

The Fold report places `0x0010163c` at `arch_cpu_idle+0x30` in the exact R4
kernel (`0x0010160c` is the function start). The instruction sequence is
`r12 = r15 - 48; lw r14, 0(r12)`. `info=0xffffffd4` is `-44`, so the effective
address calculation implies `r15=4`, while the reported `r13`/SP is valid.
`0x007d8474` is `default_idle_call+0x40` (`default_idle_call` starts at
`0x007d8434`), the return site after the call to `arch_cpu_idle`.

Cause 6 is the MyEmulator32 load-page vector; `info` is the failed load
address. This is consistent with the negative effective address above.

## Existing HALT fix and remaining boundary

The R4 Android QEMU already contains the HALT translation stores for `sr`,
`r13`, and `r15`. They are present in its compiled Android object at offsets
68, 52, and 60 in `CPUMyEmulator32State`; the matching source emits these
stores immediately before `gen_helper_halt()`. Thus the Fold failure is not
explained by that fix being absent. The desktop control survived 15m24s on
the R4 kernel and disposable rootfs; the Fold panic was at 500.570s.

The material build difference is host execution: R4 Android QEMU is built by
NDK Clang 17 for AArch64 Android API 28 and compiles TCG's AArch64 host
backend. Desktop QEMU is an x86-64 build (`cc -m64`, `-O2`) and runs TCG's
x86-64 host backend. Both use `-O2`; there is not yet evidence identifying
an optimizer, atomic-ordering, interrupt-timing, or TCG backend defect as the
cause. The fault remains unresolved pending the next Fold trace.

## Diagnostic build

Source changes are confined to the Android build's `.qemu-upstream/target/
myemulator32/{cpu.c,cpu.h,helper.c}`. With `MYEMU_HALT_TRACE=1`, QEMU keeps a
24-event ring containing HALT entry/park, IRQ wake/entry/vector, and RFE
return states (`pc`, `sr`, `r13`, `r15`, `ssp`, `usp`, pending IRQs, halt
state, and exception state). It dumps the ring once on an alignment fault or
a load-page fault to/from the low 4 KiB address range, then prints all 16
fault registers. With the variable unset, diagnostics print nothing. This
adds no kernel or rootfs changes.

Build: `ninja -C /tmp/rem-r3-android-arm64-build qemu-system-myemulator32`

Candidate:
`/tmp/rem-r4-android-halt-diagnostic/qemu-system-myemulator32`

SHA-256: `1a358026a347c08e0b30133edefa0c3c6cd59bf2874b86818bf7603f3e3e013f`

It is an Android AArch64 PIE using `/system/bin/linker64`, `$ORIGIN/lib`, and
the same shared-library dependencies as R4, including `libslirp.so.0`. Static
host checks pass. The host cannot execute this Android binary; it needs Fold
validation and is not being represented as a runtime-qualified fix.

On the Fold, replace only `qemu-system-myemulator32` in the existing R4
package, retaining its libraries, launcher, kernel, and rootfs. Launch from
the R4 directory with:

```sh
MYEMU_HALT_TRACE=1 ./launch-rem.sh
```

If the fault recurs, preserve all `MYEMU_HALT_TRACE` lines and the normal
kernel panic register report. This diagnostic does not touch `/home/dev`.

## Filesystem check

The packaged R4 rootfs is unchanged and currently passes host `e2fsck -fn`
without warnings. Earlier disposable runs showed free-block/free-inode
counter mismatches after an unclean stop; another admin-update test showed
counter mismatches despite an orderly QEMU poweroff. Those runs did not show
structural inode/directory errors, but the counter discrepancy in the
orderly-shutdown test is not explained and remains a separate filesystem
investigation. No repair was run on the packaged rootfs.
