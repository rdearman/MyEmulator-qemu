# QEMU CPU Test Images

These raw images use the current little-endian instruction encoding. The
machine installs reset vectors for `SP=0xffff` and `PC=0`.

| Image | Purpose | Expected halted state |
| --- | --- | --- |
| `li-add-halt.bin` | Basic execution | `R0=05`, `PC=0006`, `SP=ffff` |
| `branch-forward.bin` | BEQ skips one instruction | `R0=07`, `PC=000c` |
| `branch-backward.bin` | BNE takes a negative displacement once | `R0=00`, `PC=0008` |
| `jal.bin` | JAL skips one instruction | `R0=07`, `LR=0002`, `PC=0008` |
| `push-pop.bin` | Masked stack round trip | `R0=0b`, `R1=16`, `R2=21`, `LR=0002`, `SP=ffff`, `PC=0018` |

The push/pop test uses mask `0x17` (`R0`, `R1`, `R2`, `LR`). PUSH stores in ascending
order and POP restores in reverse order.
