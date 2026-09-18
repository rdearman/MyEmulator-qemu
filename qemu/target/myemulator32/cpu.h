#ifndef MYEMULATOR32_CPU_H
#define MYEMULATOR32_CPU_H

#include "cpu-qom.h"
#include "exec/cpu-defs.h"

#ifdef CONFIG_USER_ONLY
#error "MyEmulator32 only supports system emulation"
#endif

#define CPU_RESOLVING_TYPE TYPE_MYEMULATOR32_CPU
#define MMU_PHYS_IDX 0

enum {
    MYEMU32_EXCP_ILLEGAL = 1,
    MYEMU32_EXCP_PRIVILEGE,
    MYEMU32_EXCP_INSN_ALIGN,
    MYEMU32_EXCP_DATA_ALIGN,
    MYEMU32_EXCP_HALT,
};

enum {
    MYEMU32_VECTOR_ILLEGAL = 0,
    MYEMU32_VECTOR_PRIVILEGE = 1,
    MYEMU32_VECTOR_INSN_ALIGN = 2,
    MYEMU32_VECTOR_DATA_ALIGN = 3,
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
    bool halted;
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

#endif
