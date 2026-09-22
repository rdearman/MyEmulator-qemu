#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/qmp/dispatch.h"
#include "qapi/qmp/qdict.h"
#include "hw/core/cpu.h"
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
        error_setg(errp, "missing REM CPU or operation");
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
        error_setg(errp, "unknown REM debug operation '%s'", op);
        return;
    }
    *ret = QOBJECT(qdict_new());
}

void myemulator32_debug_register_qmp(void)
{
    qmp_register_command(&qmp_commands, "myemulator32-debug",
                         myemulator32_debug_qmp, 0, 0);
}
