#ifndef MYEMULATOR_CPU_H
#define MYEMULATOR_CPU_H

#include "cpu-qom.h"
#include "exec/cpu-common.h"

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

typedef struct CPUArchState {
    uint32_t pc;
    uint32_t sp;
    uint32_t lr;
    uint32_t r[4];
    uint32_t zf;
    uint32_t nf;
    uint32_t of;
    uint32_t cf;
    bool halted;
} CPUMyEmulatorState;

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
void myemulator_translate_code(CPUState *cs, TranslationBlock *tb,
                               int *max_insns, vaddr pc, void *host_pc);
bool myemulator_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                             MMUAccessType access_type, int mmu_idx,
                             bool probe, uintptr_t retaddr);
hwaddr myemulator_cpu_get_phys_addr_debug(CPUState *cs, vaddr addr);
int myemulator_cpu_gdb_read_register(CPUState *cs, GByteArray *mem_buf, int n);
int myemulator_cpu_gdb_write_register(CPUState *cs, uint8_t *mem_buf, int n);
void myemulator_cpu_do_interrupt(CPUState *cs);
bool myemulator_cpu_exec_interrupt(CPUState *cs, int interrupt_request);

#endif
