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
    case 4: case 5: case 6: case 7:
        return gdb_get_reg16(mem_buf, env->a[n - 4]);
    case 8:
        return gdb_get_reg16(mem_buf, env->lr);
    case 9:
        return gdb_get_reg16(mem_buf, env->sp);
    case 10:
        return gdb_get_reg16(mem_buf, env->pc);
    case 11:
        return gdb_get_reg8(mem_buf, env->s0);
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
    case 4: case 5: case 6: case 7:
        env->a[n - 4] = lduw_le_p(mem_buf);
        return 2;
    case 8:
        env->lr = lduw_le_p(mem_buf);
        return 2;
    case 9:
        env->sp = lduw_le_p(mem_buf);
        return 2;
    case 10:
        env->pc = lduw_le_p(mem_buf);
        return 2;
    case 11:
        env->s0 = *mem_buf & 0xff;
        return 1;
    default:
        return 0;
    }
}
