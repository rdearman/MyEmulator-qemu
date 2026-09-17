#ifndef MYEMULATOR_CPU_H
#define MYEMULATOR_CPU_H

#include "cpu-qom.h"
#include "target/myemulator/cpu-param.h"
#include "exec/cpu-defs.h"
#include "disas/dis-asm.h"

#ifdef CONFIG_USER_ONLY
#error "MyEmulator only supports system emulation"
#endif

#define CPU_RESOLVING_TYPE TYPE_MYEMULATOR_CPU

enum {
    MMU_PHYS_IDX = 0,
};

enum {
    EXCP_MYEMU_HALT = 1,
    EXCP_MYEMU_ILLEGAL = 2,
};

#define MYEMULATOR_IRQ_COUNT 7
#define MYEMULATOR_IRQ_VECTOR_BASE 0xffec
#define MYEMULATOR_IRQ_VECTOR(level) \
    (MYEMULATOR_IRQ_VECTOR_BASE + ((level) * 2))

typedef struct CPUArchState {
    uint32_t pc;
    uint32_t sp;
    uint32_t lr;
    uint32_t r[4];
    uint32_t a[4];
    uint32_t s0;
    uint8_t irq_asserted;
    bool irq_oneshot;
    uint8_t irq_after_entry;
    bool halted;
} CPUMyEmulatorState;

#define MYEMULATOR_S0_ZF 0x01
#define MYEMULATOR_S0_NF 0x02
#define MYEMULATOR_S0_CF 0x04
#define MYEMULATOR_S0_OF 0x08
#define MYEMULATOR_S0_IPL_MASK 0x70

struct ArchCPU {
    CPUState parent_obj;
    CPUMyEmulatorState env;
};

struct MyEmulatorCPUClass {
    CPUClass parent_class;
    DeviceRealize parent_realize;
    ResettablePhases parent_phases;
};

void myemulator_cpu_tcg_init(void);
bool myemulator_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                             MMUAccessType access_type, int mmu_idx,
                             bool probe, uintptr_t retaddr);
hwaddr myemulator_cpu_get_phys_addr_debug(CPUState *cs, vaddr addr);
int myemulator_cpu_gdb_read_register(CPUState *cs, GByteArray *mem_buf, int n);
int myemulator_cpu_gdb_write_register(CPUState *cs, uint8_t *mem_buf, int n);
int myemulator_print_insn(bfd_vma addr, disassemble_info *info);
void myemulator_cpu_do_interrupt(CPUState *cs);
bool myemulator_cpu_exec_interrupt(CPUState *cs, int interrupt_request);
void myemulator_cpu_set_irq(CPUState *cs, unsigned level, bool asserted);
void myemulator_cpu_set_irq_mask(CPUState *cs, unsigned mask);
void myemulator_cpu_set_irq_oneshot(CPUState *cs, bool oneshot);
void myemulator_cpu_set_irq_after_entry(CPUState *cs, unsigned level);
unsigned myemulator_cpu_highest_irq(CPUMyEmulatorState *env);

static inline void cpu_get_tb_cpu_state(CPUMyEmulatorState *env, vaddr *pc,
                                        uint64_t *cs_base, uint32_t *pflags)
{
    *pc = env->pc;
    *cs_base = 0;
    *pflags = 0;
}

#include "exec/cpu-all.h"

#endif
