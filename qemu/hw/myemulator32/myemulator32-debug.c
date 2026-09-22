#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/qmp/dispatch.h"
#include "qapi/qmp/qdict.h"
#include "hw/core/cpu.h"
#include "exec/cpu-common.h"
#include "exec/address-spaces.h"
#include "myemulator32-debug.h"
#include "target/myemulator32/cpu.h"

extern QmpCommandList qmp_commands;

static CPUState *myemulator32_debug_cpu(void)
{
    CPUState *cs;

    CPU_FOREACH(cs) {
        return cs;
    }
    return NULL;
}

static void myemulator32_debug_qmp(QDict *args, QObject **ret, Error **errp)
{
    CPUState *cs = myemulator32_debug_cpu();
    const char *op = qdict_get_try_str(args, "op");
    int64_t level;
    bool asserted;

    if (!cs || !op) {
        error_setg(errp, "missing MyEmulator32 CPU or operation");
        return;
    }
    if (!strcmp(op, "regs")) {
        CPUMyEmulator32State *env = cpu_env(cs);
        QDict *regs = qdict_new();
        qdict_put_int(regs, "pc", env->pc);
        qdict_put_int(regs, "sr", env->sr);
        qdict_put_int(regs, "usp", env->usp);
        qdict_put_int(regs, "ssp", env->ssp);
        qdict_put_int(regs, "ptbr", env->ptbr);
        qdict_put_int(regs, "mmcr", env->mmcr);
        qdict_put_int(regs, "tp", env->tp);
        qdict_put_int(regs, "timecmp", env->timecmp);
        for (unsigned i = 0; i < 16; i++) {
            char name[8];
            g_snprintf(name, sizeof(name), "r%u", i);
            qdict_put_int(regs, name, env->r[i]);
        }
        *ret = QOBJECT(regs);
        return;
    }
    if (!strcmp(op, "mem")) {
        uint64_t address = qdict_get_int(args, "address");
        uint64_t length = qdict_get_int(args, "length");
        uint8_t data[256];
        GString *hex;

        if (address > UINT32_MAX || length > sizeof(data)) {
            error_setg(errp, "memory request is outside the debug limits");
            return;
        }
        if (cpu_memory_rw_debug(cs, address, data, length, 0) < 0) {
            error_setg(errp, "guest memory read failed");
            return;
        }
        hex = g_string_sized_new(length * 2);
        for (uint64_t i = 0; i < length; i++) {
            g_string_append_printf(hex, "%02x", data[i]);
        }
        *ret = QOBJECT(qdict_new());
        qdict_put_str(qobject_to(QDict, *ret), "data", hex->str);
        g_string_free(hex, true);
        return;
    }
    if (!strcmp(op, "physmem")) {
        uint64_t address = qdict_get_int(args, "address");
        uint64_t length = qdict_get_int(args, "length");
        uint8_t data[256];
        GString *hex;

        if (length > sizeof(data)) {
            error_setg(errp, "physical memory request is too large");
            return;
        }
        if (address_space_read(&address_space_memory, address,
                               MEMTXATTRS_UNSPECIFIED, data, length) != MEMTX_OK) {
            error_setg(errp, "physical memory read failed");
            return;
        }
        hex = g_string_sized_new(length * 2);
        for (uint64_t i = 0; i < length; i++) {
            g_string_append_printf(hex, "%02x", data[i]);
        }
        *ret = QOBJECT(qdict_new());
        qdict_put_str(qobject_to(QDict, *ret), "data", hex->str);
        g_string_free(hex, true);
        return;
    }
    if (!strcmp(op, "irq")) {
        level = qdict_get_int(args, "level");
        asserted = qdict_get_bool(args, "asserted");
        if (level < 1 || level > 7) {
            error_setg(errp, "IRQ level must be 1 through 7");
            return;
        }
        myemulator32_cpu_set_irq(cs, level, asserted);
    } else if (!strcmp(op, "nmi")) {
        asserted = qdict_get_bool(args, "asserted");
        myemulator32_cpu_set_nmi(cs, asserted);
    } else {
        error_setg(errp, "unknown MyEmulator32 debug operation '%s'", op);
        return;
    }
    *ret = QOBJECT(qdict_new());
}

void myemulator32_debug_register_qmp(void)
{
    qmp_register_command(&qmp_commands, "myemulator32-debug",
                         myemulator32_debug_qmp, 0, 0);
}
