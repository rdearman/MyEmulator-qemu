#ifndef OPCODE_MYEMULATOR2_H
#define OPCODE_MYEMULATOR2_H

/* MyEmulator2 has one fixed 32-bit little-endian instruction word.  The
   assembler and disassembler share these field definitions. */
#define MYEMU2_OP(x) ((x) << 26)
#define MYEMU2_RD(x) ((x) << 21)
#define MYEMU2_RA(x) ((x) << 16)
#define MYEMU2_RB(x) ((x) << 11)
#define MYEMU2_FN(x) ((x) << 6)
#define MYEMU2_IMM12(x) ((x) & 0xfff)

enum myemulator2_r_op {
  MYEMU2_ADD, MYEMU2_ADC, MYEMU2_SUB, MYEMU2_SBC,
  MYEMU2_MUL, MYEMU2_MULH, MYEMU2_MULHU, MYEMU2_DIV, MYEMU2_DIVU,
  MYEMU2_REM, MYEMU2_REMU, MYEMU2_AND, MYEMU2_OR, MYEMU2_XOR, MYEMU2_NOT,
  MYEMU2_SLL, MYEMU2_SRL, MYEMU2_SRA, MYEMU2_ROL, MYEMU2_ROR,
  MYEMU2_SEQ, MYEMU2_SNE, MYEMU2_SLT, MYEMU2_SGE, MYEMU2_SLTU, MYEMU2_SGEU
};

enum myemulator2_sysreg {
  MYEMU2_SR, MYEMU2_USP, MYEMU2_SSP, MYEMU2_VBR, MYEMU2_PTBR,
  MYEMU2_MMCR, MYEMU2_TIME_LO, MYEMU2_TIME_HI, MYEMU2_TIMECMP_LO,
  MYEMU2_TIMECMP_HI, MYEMU2_TP, MYEMU2_DFSP
};

#endif
