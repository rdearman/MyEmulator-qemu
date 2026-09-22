#ifndef MYEMULATOR32_CPU_H
#define MYEMULATOR32_CPU_H

#include "cpu-qom.h"
#include "exec/cpu-defs.h"
#include "qemu/timer.h"

#ifdef CONFIG_USER_ONLY
#error "MyEmulator32 only supports system emulation"
#endif

#define CPU_RESOLVING_TYPE TYPE_MYEMULATOR32_CPU
#define MMU_SUPERVISOR_IDX 0
#define MMU_USER_IDX 1

enum {
    /* Keep QEMU-internal exception indices separate from the architectural
     * vector numbers stored by myemu32_enter_exception().  In particular,
     * vector 7 is instruction-protection, while the old sequential enum
     * assigned 7 to MYEMU32_EXCP_NMI. */
    MYEMU32_EXCP_ILLEGAL = 0x100,
    MYEMU32_EXCP_PRIVILEGE,
    MYEMU32_EXCP_INSN_ALIGN,
    MYEMU32_EXCP_DATA_ALIGN,
    MYEMU32_EXCP_HALT,
    MYEMU32_EXCP_IRQ,
    MYEMU32_EXCP_NMI,
    MYEMU32_EXCP_DOUBLE_FAULT,
};

enum {
    MYEMU32_VECTOR_ILLEGAL = 0,
    MYEMU32_VECTOR_PRIVILEGE = 1,
    MYEMU32_VECTOR_INSN_ALIGN = 2,
    MYEMU32_VECTOR_DATA_ALIGN = 3,
    MYEMU32_VECTOR_INSN_PAGE = 4,
    MYEMU32_VECTOR_INSN_PROT = 5,
    MYEMU32_VECTOR_LOAD_PAGE = 6,
    MYEMU32_VECTOR_LOAD_PROT = 7,
    MYEMU32_VECTOR_STORE_PAGE = 8,
    MYEMU32_VECTOR_STORE_PROT = 9,
    MYEMU32_VECTOR_DIV_ZERO = 10,
    MYEMU32_VECTOR_ARITH_OVERFLOW = 11,
    MYEMU32_VECTOR_SYSCALL = 12,
    MYEMU32_VECTOR_BREAKPOINT = 13,
    MYEMU32_VECTOR_NMI = 14,
    MYEMU32_VECTOR_DOUBLE_FAULT = 15,
    MYEMU32_VECTOR_IRQ1 = 16,
};

#define MYEMU32_SR_CF  (1u << 0)
#define MYEMU32_SR_OF  (1u << 1)
#define MYEMU32_SR_IPL_MASK (7u << 2)
#define MYEMU32_SR_S   (1u << 5)

#define MYEMU32_FRAME_SIZE 16

typedef struct CPUArchState {
    uint32_t r[16];
    uint32_t pc;
    uint32_t sr;
    uint32_t usp;
    uint32_t ssp;
    uint32_t vbr;
    uint32_t ptbr;
    uint32_t mmcr;
    uint32_t dfsp;
    uint32_t tp;
    uint64_t timecmp;
    uint64_t timecmp_shadow;
    uint64_t time_base_ns;
    uint32_t time_hi_latch;
    bool timecmp_armed;
    bool time_irq_asserted;
    QEMUTimer *time_timer;
    bool halted;
    uint8_t irq_asserted;
    bool nmi_active;
} CPUMyEmulator32State;

struct ArchCPU {
    CPUState parent_obj;
    CPUMyEmulator32State env;
};

struct MyEmulator32CPUClass {
    CPUClass parent_class;
    DeviceRealize parent_realize;
    ResettablePhases parent_phases;
};

void myemulator32_cpu_tcg_init(void);
bool myemulator32_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                               MMUAccessType access_type, int mmu_idx,
                               bool probe, uintptr_t retaddr);
void myemulator32_cpu_set_irq(CPUState *cs, unsigned level, bool asserted);
void myemulator32_cpu_set_nmi(CPUState *cs, bool asserted);
uint64_t myemulator32_cpu_time_us(CPUMyEmulator32State *env);
void myemulator32_cpu_program_timecmp(CPUMyEmulator32State *env);
void myemulator32_cpu_dump_state(CPUState *cs, FILE *f, int flags);

static inline void cpu_get_tb_cpu_state(CPUMyEmulator32State *env,
                                        vaddr *pc, uint64_t *cs_base,
                                        uint32_t *pflags)
{
    *pc = env->pc;
    *cs_base = 0;
    *pflags = env->sr & MYEMU32_SR_S;
}

#include "exec/cpu-all.h"

#define CPU_INTERRUPT_NMI CPU_INTERRUPT_TGT_EXT_3

#endif
