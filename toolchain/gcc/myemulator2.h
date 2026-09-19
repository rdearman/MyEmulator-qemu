/* GCC target definitions for MyEmulator2.  This file is maintained as a
   project fragment and is installed into gcc/config/myemulator2 by the
   reproducible source-preparation script. */
#ifndef GCC_MYEMULATOR2_H
#define GCC_MYEMULATOR2_H

#define STARTFILE_SPEC ""
#define ENDFILE_SPEC ""
#define LIB_SPEC ""
#define LINK_SPEC "%{static:-Bstatic}"

#define INT_TYPE_SIZE 32
#define SHORT_TYPE_SIZE 16
#define LONG_TYPE_SIZE 32
#define LONG_LONG_TYPE_SIZE 64
#define DEFAULT_SIGNED_CHAR 0
#define SIZE_TYPE "unsigned int"
#define PTRDIFF_TYPE "int"
#define WCHAR_TYPE "unsigned int"
#define WCHAR_TYPE_SIZE 32

#define MYEMU2_R0 0
#define MYEMU2_R1 1
#define MYEMU2_R2 2
#define MYEMU2_R3 3
#define MYEMU2_R4 4
#define MYEMU2_R5 5
#define MYEMU2_R6 6
#define MYEMU2_R7 7
#define MYEMU2_R8 8
#define MYEMU2_R9 9
#define MYEMU2_R10 10
#define MYEMU2_R11 11
#define MYEMU2_R12 12
#define MYEMU2_R15 15
#define MYEMU2_SP 13
#define MYEMU2_LR 14
#define MYEMU2_FP 15
#define MYEMU2_AP 16
#define MYEMU2_FRAME 15

#define REGISTER_NAMES { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", \
  "r8", "r9", "r10", "r11", "r12", "sp", "lr", "r15", "?ap" }
#define FIRST_PSEUDO_REGISTER 17

enum reg_class { NO_REGS, GENERAL_REGS, ALL_REGS, LIM_REG_CLASSES };
#define REG_CLASS_CONTENTS { { 0 }, { 0x0001FFFE }, { 0x0001FFFF } }
#define N_REG_CLASSES LIM_REG_CLASSES
#define REG_CLASS_NAMES { "NO_REGS", "GENERAL_REGS", "ALL_REGS" }
#define REGNO_REG_CLASS(R) ((R) == MYEMU2_R0 ? NO_REGS : GENERAL_REGS)

/* r0 is architectural zero; r13, r14 and r15 are reserved for SP, LR and
   the compiler frame base.  r12 is retained as a prologue scratch. */
#define FIXED_REGISTERS { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 }
#define CALL_USED_REGISTERS { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 }
#define REGNO_OK_FOR_BASE_P(N) ((N) < 16 && (N) != MYEMU2_R0 && (N) != MYEMU2_LR)
#define REGNO_OK_FOR_INDEX_P(N) 0
#define HARD_REGNO_OK_FOR_BASE_P(N) REGNO_OK_FOR_BASE_P(N)
#define BASE_REG_CLASS GENERAL_REGS
#define INDEX_REG_CLASS NO_REGS

#define BITS_BIG_ENDIAN 0
#define BYTES_BIG_ENDIAN 0
#define WORDS_BIG_ENDIAN 0
#define UNITS_PER_WORD 4
#define BITS_PER_WORD 32
#define POINTER_SIZE 32
#define FUNCTION_BOUNDARY 32
#define STACK_BOUNDARY 128
#define PARM_BOUNDARY 32
#define EMPTY_FIELD_BOUNDARY 32
#define BIGGEST_ALIGNMENT 32
#define FASTEST_ALIGNMENT 32
#define STRUCTURE_SIZE_BOUNDARY 8
#define STRICT_ALIGNMENT 1
#define MOVE_MAX 4
#define SLOW_BYTE_ACCESS 0
#define MAX_REGS_PER_ADDRESS 1
#define Pmode SImode
#define FUNCTION_MODE QImode
#define STACK_POINTER_REGNUM MYEMU2_SP
#define FRAME_POINTER_REGNUM MYEMU2_FRAME
#define ARG_POINTER_REGNUM MYEMU2_AP
#define HARD_FRAME_POINTER_REGNUM MYEMU2_FP
#define ELIMINABLE_REGS {{ FRAME_POINTER_REGNUM, STACK_POINTER_REGNUM }, \
                         { ARG_POINTER_REGNUM, STACK_POINTER_REGNUM }}
#define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET) \
  ((OFFSET) = myemulator2_initial_elimination_offset ((FROM), (TO)))
#define FUNCTION_ARG_REGNO_P(R) ((R) >= MYEMU2_R1 && (R) <= MYEMU2_R4)
#define CUMULATIVE_ARGS unsigned int
#define INIT_CUMULATIVE_ARGS(CUM,FNTYPE,LIBNAME,FNDECL,N_NAMED_ARGS) \
  (CUM = MYEMU2_R1)
#define EPILOGUE_USES(R) ((R) == MYEMU2_LR)
#define STACK_GROWS_DOWNWARD 1
#define FRAME_GROWS_DOWNWARD 1
#define ACCUMULATE_OUTGOING_ARGS 1
#define OUTGOING_REG_PARM_STACK_SPACE(FNTYPE) 1
#define STACK_PARMS_IN_REG_PARM_AREA
/* Reserve one word for each of the four register argument slots.  Stack
   arguments therefore start after the register-save area and cannot overlap
   the fixed LR/frame header. */
#define REG_PARM_STACK_SPACE(FNDECL) (4 * UNITS_PER_WORD)
#define STACK_POINTER_OFFSET (4 * UNITS_PER_WORD)
/* The target prologue reserves words 0 and 4 of the incoming frame for the
   saved LR and frame register.  Stack-passed arguments begin immediately
   after that fixed header. */
#define FIRST_PARM_OFFSET(FNDECL) (4 * UNITS_PER_WORD)

#define ASM_COMMENT_START "#"
#define ASM_APP_ON ""
#define ASM_APP_OFF ""
#define TEXT_SECTION_ASM_OP "\t.text"
#define DATA_SECTION_ASM_OP "\t.data"
#define BSS_SECTION_ASM_OP "\t.bss"
#define GLOBAL_ASM_OP "\t.global\t"
#define ASM_OUTPUT_ALIGN(FILE, POWER) fprintf ((FILE), "\t.p2align\t%d\n", (POWER))
#define TARGET_ASM_FILE_START_FILE_DIRECTIVE true

#define TARGET_CPU_CPP_BUILTINS() do { \
  builtin_define_std ("myemulator2"); \
  builtin_define_std ("MYEMULATOR2"); \
  builtin_define ("__MYEMULATOR2__"); \
} while (0)

#define CASE_VECTOR_MODE SImode
#define CASE_VECTOR_PC_RELATIVE 1
#define HAS_LONG_UNCOND_BRANCH true
#define NO_FUNCTION_CSE 1
#define TRAMPOLINE_SIZE 16
#define TRAMPOLINE_ALIGNMENT 32
#define DEFAULT_SIGNED_CHAR 0
#define LOAD_EXTEND_OP(MEM) ZERO_EXTEND
#define TARGET_DEFAULT 0
#define FUNCTION_PROFILER(FILE, LABEL) do { } while (0)

#define INCOMING_RETURN_ADDR_RTX gen_rtx_REG (Pmode, MYEMU2_LR)
#define RETURN_ADDR_RTX(COUNT, FRAME) ((COUNT) == 0 ? gen_rtx_REG (Pmode, MYEMU2_LR) : NULL_RTX)

#endif
