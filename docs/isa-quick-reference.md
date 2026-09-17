# MyEmulator ISA quick reference

Instructions are 16-bit little-endian words; ordinary PC increments by 2.
Branches and JAL use `target = PC+2 + sign_extend(disp8)*2`.

```text
LD/ ST rd,[An+disp8]       LI Rd,#imm8
ADD/SUB/AND/OR/XOR/SHL/SHR Rd,Rn,#imm8
ADD/SUB/AND/OR/XOR/SHL/SHR Rd,Rn  (0x77xx register forms)
CMP Rd,Rn
JAL label                  BR/BEQ/BNE/BLT/BGE/BLTU/BGEU label
LDA An,Rx,Ry               GTA Rx,Ry,An
MVA A0-A3|LR|SP,A0-A3|LR|SP
ADA An,#signed8             GF/SF R0-R3
PUSH/POP {R0,R1,R2,R3,LR}  RET  RTI  HALT
```

Encoding anchors: `LI=0x1`, `ADD=0x3`, `SUB=0x4`, `JAL=0x5`, branches=`0x6`,
address/status=`0x7`, `CMP=0x8`, `PUSH=0xe`, `POP/extensions=0xf`.
`RET=0xf040`, `RTI=0xf060`, `HALT=0xf080`. `S0`: ZF bit 0, NF bit 1, CF
bit 2, OF bit 3, IPL bits 4-6. The `0x77xx` ALU selector is in bits 7-4;
selectors 0-6 are ADD, SUB, AND, OR, XOR, SHL, SHR.
