# R4 Fold Make fault triage

## Fold capture

At 693.320 seconds, the Fold reported `MYEMU_BAD_USER_FAULT` for PID 48,
`comm=make`, with PC `0x02074afc`, store-page cause 8, fault address
`0x3f65ffe8`, SP `0x3f65ffb8`, FP `0x3f66005c`, and LR `0x0206f8e0`.
The kernel's VMA diagnostic was all zeroes, meaning `find_vma(mm, addr)`
returned NULL. This is distinct from the earlier PID 0 idle fault.

## Exact binary identification

The captured 16 bytes at PC are:

```
00 40 2d 0d 90 10 ad 05 30 40 cd 0d 34 40 8d 0d
```

They match the bytes at `0x02074afc` in the R4 rootfs's `/usr/bin/make`,
extracted read-only to `/tmp/rem-r4-rootfs-make`:

```
SHA256 f474110061701a845f11bee32fe09a167b8334fb088940ad33f64452a4630527
GNU Make 4.4.1, static REM ELF32, 1,138,772 bytes
```

The candidate `/tmp/rem-r4-make-fix-work/qualified/make.new` is a different
binary (SHA256
`f5a66f2576603723c1745af4b485a9c3d44af9e020ca59640f4a2ae6b1100f79`,
1,373,192 bytes). Its `.text` ends before the reported PC and its bytes at
that offset do not match the Fold capture. Therefore the capture is from the
old `/usr/bin/make` image, not this `make.new` artifact, barring prior
execution-memory corruption. The shell command in the capture was `make`,
which normally resolves to `/usr/bin/make`; the Fold report that `make.new`
was installed/running conflicts with the binary evidence. Verify both hashes
on Fold before the next comparison.

## Disassembly and address calculation

The old Make symbol table places PC at `__muldf3+0x34`. The instruction is:

```
02074af8: subi r13,r13,144
02074afc: sw r14,48(r13)
02074b00: sw r12,52(r13)
```

Thus `EA = r13 + 48 = 0x3f65ffb8 + 0x30 = 0x3f65ffe8`.
LR `0x0206f8e0` is the delay slot after the `jal __muldf3` at
`frexp+0x108` (`frexp` starts `0x0206f7d4`). The caller is scaling a
subnormal double by `2^64` before retrying `frexp`; the old Make binary has
entered a floating-point runtime path.

At function entry the frame pointer is about `0x3f66005c`; `__muldf3` saves
registers and reserves 144 bytes, moving SP down through the `0x3f660000`
page boundary. The faulting store is a normal callee-save operation, not an
unaligned access or an obviously corrupted frame. Its address is in the
first lower stack page. However, the kernel reports no VMA at all. A normal
stack-growth fault should find the grow-down stack VMA above the address and
expand it. The supplied registers do not establish why that VMA is absent.

## Classification / status

- **Make defect:** the captured binary is the old build, already known to
  return empty wildcard results and miss ordinary pattern rules. A qualified
  replacement exists at `/tmp/rem-r4-make-fix-work/qualified/make.new`.
- **Kernel VM / mapping:** the zero VMA report for a coherent stack-frame
  store is consistent with a missing/corrupt stack VMA or stack setup/growth
  defect. It is not enough evidence to distinguish kernel VMA loss from
  earlier process memory-management damage. Do not label it fixed.
- **ABI/compiler:** no evidence of a bad frame or ABI mismatch in this capture;
  the decoded instruction and SP/FP relationship are coherent.
- **QEMU/HALT:** no relation demonstrated. The Fold survived to 693.320 s,
  beyond the prior 500.570 s idle panic; one run is not a HALT qualification.

## Available reproduction and next Fold discriminator

`docs/REM_NATIVE_MAKE_R4.md` records disposable R4 qualification of
`make.new`: wildcard, pattern rule, `$@/$</$*/$^`, timestamp rebuilds,
phony targets, and multi-object prerequisites passed. The exact Fold
REMEDIT Makefile/source tree was not present in the host workspace. Run the
Fold workload with explicit binaries and hashes before comparing:

```
sha256sum /usr/bin/make ./make.new
./make.new list
./make.new clean
./make.new
./make.new test
```

If `make.new` faults with the same PC and bytes, preserve its hash and report
that new capture: it would imply code/data corruption or a broader kernel
mapping defect, since its current linked text does not contain that code at
the reported address. If only `/usr/bin/make` faults, the replacement avoids
the old Make path but does not itself repair the kernel's missing-VMA
condition. Kernel owner should inspect stack VMA presence at exec and when
the first page below `0x3f660000` faults.

No R4 image, source rootfs, kernel, or QEMU was modified for this triage.
