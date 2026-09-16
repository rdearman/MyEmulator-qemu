#include "qemu/osdep.h"
#include "gdbstub/helpers.h"
#include "cpu.h"

int myemulator_cpu_gdb_read_register(CPUState *cs, GByteArray *mem_buf, int n)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    if (n < 4) {
        return gdb_get_reg8(mem_buf, env->r[n]);
    }
    switch (n) {
    case 4:
        return gdb_get_reg8(mem_buf, env->lr);
    case 5:
        return gdb_get_reg16(mem_buf, env->sp);
    case 6:
        return gdb_get_reg16(mem_buf, env->pc);
    case 7:
        return gdb_get_reg8(mem_buf, (env->zf << 0) |
                                     (env->nf << 1) |
                                     (env->of << 2) |
                                     (env->cf << 3));
    default:
        return 0;
    }
}

int myemulator_cpu_gdb_write_register(CPUState *cs, uint8_t *mem_buf, int n)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    if (n < 4) {
        env->r[n] = *mem_buf & 0xff;
        return 1;
    }
    switch (n) {
    case 4:
        env->lr = *mem_buf & 0xff;
        return 1;
    case 5:
        env->sp = lduw_le_p(mem_buf);
        return 2;
    case 6:
        env->pc = lduw_le_p(mem_buf);
        return 2;
    case 7:
        env->zf = (*mem_buf >> 0) & 1;
        env->nf = (*mem_buf >> 1) & 1;
        env->of = (*mem_buf >> 2) & 1;
        env->cf = (*mem_buf >> 3) & 1;
        return 1;
    default:
        return 0;
    }
}
