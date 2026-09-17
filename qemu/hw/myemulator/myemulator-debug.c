#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "exec/cpu-common.h"
#include "exec/memory.h"
#include "hw/core/cpu.h"
#include "qapi/error.h"
#include "qapi/qmp/dispatch.h"
#include "qapi/qmp/qdict.h"
#include "qapi/qmp/qlist.h"
#include "qemu/cutils.h"
#include "sysemu/reset.h"
#include "sysemu/runstate.h"
#include "target/myemulator/cpu.h"
#include "myemulator-debug.h"

extern QmpCommandList qmp_commands;

static GHashTable *myemulator_debug_breakpoints;

static CPUState *myemulator_debug_cpu(void)
{
    CPUState *cpu;

    CPU_FOREACH(cpu) {
        return cpu;
    }
    return NULL;
}

static QDict *myemulator_debug_registers(CPUState *cpu)
{
    CPUMyEmulatorState *env = cpu_env(cpu);
    QDict *regs = qdict_new();

    qdict_put_int(regs, "r0", env->r[0] & 0xff);
    qdict_put_int(regs, "r1", env->r[1] & 0xff);
    qdict_put_int(regs, "r2", env->r[2] & 0xff);
    qdict_put_int(regs, "r3", env->r[3] & 0xff);
    qdict_put_int(regs, "a0", env->a[0] & 0xffff);
    qdict_put_int(regs, "a1", env->a[1] & 0xffff);
    qdict_put_int(regs, "a2", env->a[2] & 0xffff);
    qdict_put_int(regs, "a3", env->a[3] & 0xffff);
    qdict_put_int(regs, "lr", env->lr & 0xffff);
    qdict_put_int(regs, "sp", env->sp & 0xffff);
    qdict_put_int(regs, "pc", env->pc & 0xffff);
    qdict_put_int(regs, "s0", env->s0 & 0xff);
    return regs;
}

static QDict *myemulator_debug_state(CPUState *cpu)
{
    CPUMyEmulatorState *env = cpu_env(cpu);
    QDict *state = qdict_new();
    QDict *flags = qdict_new();

    qdict_put(state, "registers", QOBJECT(myemulator_debug_registers(cpu)));
    qdict_put_bool(state, "running", runstate_is_running());
    qdict_put_bool(state, "halted", env->halted);
    qdict_put_int(state, "pc", env->pc & 0xffff);
    qdict_put_int(state, "s0", env->s0 & 0xff);
    qdict_put_bool(flags, "zf", !!(env->s0 & MYEMULATOR_S0_ZF));
    qdict_put_bool(flags, "nf", !!(env->s0 & MYEMULATOR_S0_NF));
    qdict_put_bool(flags, "cf", !!(env->s0 & MYEMULATOR_S0_CF));
    qdict_put_bool(flags, "of", !!(env->s0 & MYEMULATOR_S0_OF));
    qdict_put_int(flags, "ipl", (env->s0 & MYEMULATOR_S0_IPL_MASK) >> 4);
    qdict_put(state, "flags", QOBJECT(flags));
    return state;
}

static int myemulator_debug_read_memory(bfd_vma address, bfd_byte *buffer,
                                        int length, disassemble_info *info)
{
    MemTxResult result;

    result = address_space_read(&address_space_memory, address,
                                MEMTXATTRS_UNSPECIFIED, buffer, length);
    return result == MEMTX_OK ? 0 : EIO;
}

static void myemulator_debug_memory_error(int status, bfd_vma address,
                                          disassemble_info *info)
{
}

static char *myemulator_debug_disassemble_one(CPUState *cpu, uint16_t address,
                                              int *size)
{
    char *text = NULL;
    size_t text_size = 0;
    FILE *stream;
    disassemble_info info;

    stream = open_memstream(&text, &text_size);
    if (!stream) {
        return NULL;
    }
    memset(&info, 0, sizeof(info));
    info.stream = stream;
    info.fprintf_func = fprintf;
    info.read_memory_func = myemulator_debug_read_memory;
    info.memory_error_func = myemulator_debug_memory_error;
    *size = myemulator_print_insn(address, &info);
    fclose(stream);
    if (*size < 0) {
        g_free(text);
        return NULL;
    }
    return text;
}

static QDict *myemulator_debug_disassemble(CPUState *cpu, uint16_t address,
                                           unsigned count)
{
    QList *instructions = qlist_new();
    unsigned i;

    for (i = 0; i < count; i++) {
        QDict *instruction = qdict_new();
        int size;
        char *text = myemulator_debug_disassemble_one(cpu, address, &size);

        if (!text) {
            qobject_unref(instruction);
            break;
        }
        qdict_put_int(instruction, "address", address);
        qdict_put_int(instruction, "size", size);
        qdict_put_str(instruction, "text", text);
        qlist_append(instructions, QOBJECT(instruction));
        g_free(text);
        address = (address + size) & 0xffff;
    }

    QDict *result = qdict_new();
    qdict_put(result, "instructions", QOBJECT(instructions));
    return result;
}

static bool myemulator_debug_parse_hex(const char *text, uint8_t **data,
                                       size_t *length)
{
    size_t len = strlen(text);
    uint8_t *buffer;

    if (len & 1) {
        return false;
    }
    buffer = g_malloc(len / 2);
    for (size_t i = 0; i < len; i += 2) {
        int hi = g_ascii_xdigit_value(text[i]);
        int lo = g_ascii_xdigit_value(text[i + 1]);
        if (hi < 0 || lo < 0) {
            g_free(buffer);
            return false;
        }
        buffer[i / 2] = (hi << 4) | lo;
    }
    *data = buffer;
    *length = len / 2;
    return true;
}

static void myemulator_debug_qmp(QDict *args, QObject **ret, Error **errp)
{
    CPUState *cpu = myemulator_debug_cpu();
    const char *op = qdict_get_try_str(args, "op");
    QDict *result;

    if (!cpu) {
        error_setg(errp, "MyEmulator CPU is not available");
        return;
    }
    if (!op) {
        error_setg(errp, "missing debugger operation");
        return;
    }

    if (!strcmp(op, "registers")) {
        *ret = QOBJECT(myemulator_debug_state(cpu));
        return;
    }
    if (!strcmp(op, "reset")) {
        qemu_system_reset(SHUTDOWN_CAUSE_GUEST_RESET);
        cpu_reset(cpu);
        vm_stop(RUN_STATE_PAUSED);
        *ret = QOBJECT(myemulator_debug_state(cpu));
        return;
    }
    if (!strcmp(op, "write-register")) {
        const char *name = qdict_get_try_str(args, "name");
        int64_t value = qdict_get_int(args, "value");
        CPUMyEmulatorState *env = cpu_env(cpu);
        uint32_t max;

        if (!name || value < 0) {
            error_setg(errp, "invalid register write");
            return;
        }
        if (!g_ascii_strcasecmp(name, "s0") ||
            (!g_ascii_strcasecmp(name, "r0") ||
             !g_ascii_strcasecmp(name, "r1") ||
             !g_ascii_strcasecmp(name, "r2") ||
             !g_ascii_strcasecmp(name, "r3"))) {
            max = 0xff;
        } else {
            max = 0xffff;
        }
        if ((uint64_t)value > max) {
            error_setg(errp, "value does not fit register %s", name);
            return;
        }
        if (name[0] == 'r' || name[0] == 'R') {
            unsigned index = name[1] - '0';
            if (index > 3 || name[2] != '\0') {
                error_setg(errp, "unknown register %s", name);
                return;
            }
            env->r[index] = value;
        } else if (name[0] == 'a' || name[0] == 'A') {
            unsigned index = name[1] - '0';
            if (index > 3 || name[2] != '\0') {
                error_setg(errp, "unknown register %s", name);
                return;
            }
            env->a[index] = value;
        } else if (!g_ascii_strcasecmp(name, "lr")) {
            env->lr = value;
        } else if (!g_ascii_strcasecmp(name, "sp")) {
            env->sp = value;
        } else if (!g_ascii_strcasecmp(name, "pc")) {
            cpu_set_pc(cpu, value);
        } else if (!g_ascii_strcasecmp(name, "s0")) {
            env->s0 = value;
        } else {
            error_setg(errp, "unknown register %s", name);
            return;
        }
        *ret = QOBJECT(myemulator_debug_state(cpu));
        return;
    }
    if (!strcmp(op, "step")) {
        cpu_single_step(cpu, SSTEP_ENABLE);
        vm_start();
        *ret = QOBJECT(qdict_new());
        return;
    }
    if (!strcmp(op, "continue")) {
        cpu_single_step(cpu, 0);
        vm_start();
        *ret = QOBJECT(myemulator_debug_state(cpu));
        return;
    }
    if (!strcmp(op, "stop")) {
        vm_stop(RUN_STATE_PAUSED);
        *ret = QOBJECT(myemulator_debug_state(cpu));
        return;
    }
    if (!strcmp(op, "read-memory")) {
        uint64_t address = qdict_get_int(args, "address");
        uint64_t length = qdict_get_int(args, "length");
        uint8_t *buffer;
        char *hex;
        MemTxResult tx;

        if (length > 0x10000 || address > 0xffff ||
            length > 0x10000 - address) {
            error_setg(errp, "memory range is outside the 64 KiB address space");
            return;
        }
        buffer = g_malloc(length);
        tx = address_space_read(&address_space_memory, address,
                                MEMTXATTRS_UNSPECIFIED, buffer, length);
        if (tx != MEMTX_OK) {
            g_free(buffer);
            error_setg(errp, "memory read failed");
            return;
        }
        hex = g_malloc(length * 2 + 1);
        for (uint64_t i = 0; i < length; i++) {
            sprintf(hex + i * 2, "%02x", buffer[i]);
        }
        result = qdict_new();
        qdict_put_int(result, "address", address);
        qdict_put_str(result, "data", hex);
        g_free(hex);
        g_free(buffer);
        *ret = QOBJECT(result);
        return;
    }
    if (!strcmp(op, "write-memory")) {
        const char *hex = qdict_get_try_str(args, "data");
        uint64_t address = qdict_get_int(args, "address");
        uint8_t *buffer;
        size_t length;
        MemTxResult tx;

        if (!hex || !myemulator_debug_parse_hex(hex, &buffer, &length) ||
            address > 0xffff || length > 0x10000 - address) {
            error_setg(errp, "invalid memory write");
            return;
        }
        tx = address_space_write(&address_space_memory, address,
                                 MEMTXATTRS_UNSPECIFIED, buffer, length);
        g_free(buffer);
        if (tx != MEMTX_OK) {
            error_setg(errp, "memory write failed");
            return;
        }
        *ret = QOBJECT(myemulator_debug_state(cpu));
        return;
    }
    if (!strcmp(op, "disassemble")) {
        uint64_t address = qdict_get_int(args, "address");
        uint64_t count = qdict_get_int(args, "count");

        if (address > 0xffff || count > 0x10000 / 2) {
            error_setg(errp, "invalid disassembly range");
            return;
        }
        *ret = QOBJECT(myemulator_debug_disassemble(cpu, address, count));
        return;
    }
    if (!strcmp(op, "break")) {
        uint64_t address = qdict_get_int(args, "address");
        int retcode;

        if (address > 0xffff || (address & 1)) {
            error_setg(errp, "breakpoint must be an aligned 16-bit address");
            return;
        }
        retcode = cpu_breakpoint_insert(cpu, address, BP_CPU, NULL);
        if (retcode < 0) {
            error_setg(errp, "could not insert breakpoint");
            return;
        }
        if (!myemulator_debug_breakpoints) {
            myemulator_debug_breakpoints = g_hash_table_new(g_direct_hash,
                                                            g_direct_equal);
        }
        g_hash_table_add(myemulator_debug_breakpoints,
                         GUINT_TO_POINTER((guint)address));
        *ret = QOBJECT(myemulator_debug_state(cpu));
        return;
    }
    if (!strcmp(op, "delete-break")) {
        uint64_t address = qdict_get_int(args, "address");

        cpu_breakpoint_remove(cpu, address, BP_CPU);
        if (myemulator_debug_breakpoints) {
            g_hash_table_remove(myemulator_debug_breakpoints,
                                GUINT_TO_POINTER((guint)address));
        }
        *ret = QOBJECT(myemulator_debug_state(cpu));
        return;
    }
    if (!strcmp(op, "breakpoints")) {
        QList *list = qlist_new();
        GHashTableIter iter;
        gpointer key;

        if (myemulator_debug_breakpoints) {
            g_hash_table_iter_init(&iter, myemulator_debug_breakpoints);
            while (g_hash_table_iter_next(&iter, &key, NULL)) {
                qlist_append_int(list, GPOINTER_TO_UINT(key));
            }
        }
        result = qdict_new();
        qdict_put(result, "breakpoints", QOBJECT(list));
        *ret = QOBJECT(result);
        return;
    }

    error_setg(errp, "unknown MyEmulator debugger operation '%s'", op);
}

void myemulator_debug_register_qmp(void)
{
    qmp_register_command(&qmp_commands, "myemulator-debug",
                         myemulator_debug_qmp, 0, 0);
}
