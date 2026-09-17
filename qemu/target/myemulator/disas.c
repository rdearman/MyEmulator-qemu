#include "qemu/osdep.h"
#include "disas/dis-asm.h"
#include "cpu.h"

static const char * const data_regs[] = { "r0", "r1", "r2", "r3" };
static const char * const address_regs[] = { "a0", "a1", "a2", "a3" };
static const char * const wide_regs[] = {
    "a0", "a1", "a2", "a3", "lr", "sp"
};
static const char * const alu_names[] = {
    "add", "sub", "and", "or", "xor", "shl", "shr"
};

static int8_t sign8(uint8_t value)
{
    return (int8_t)value;
}

static uint16_t branch_target(bfd_vma address, uint8_t displacement)
{
    return (address + 2 + sign8(displacement) * 2) & 0xffff;
}

static void print_memory_operand(disassemble_info *info, unsigned reg,
                                 uint8_t displacement)
{
    int8_t signed_disp = sign8(displacement);

    if (signed_disp == 0) {
        info->fprintf_func(info->stream, "[%s]", address_regs[reg]);
    } else {
        info->fprintf_func(info->stream, "[%s%+d]", address_regs[reg],
                           signed_disp);
    }
}

static void print_push_pop(disassemble_info *info, const char *mnemonic,
                           uint8_t mask)
{
    bool first = true;

    info->fprintf_func(info->stream, "%s {", mnemonic);
    for (unsigned reg = 0; reg < 5; reg++) {
        if (mask & (1U << reg)) {
            if (!first) {
                info->fprintf_func(info->stream, ",");
            }
            info->fprintf_func(info->stream, "%s",
                               reg == 4 ? "lr" : data_regs[reg]);
            first = false;
        }
    }
    info->fprintf_func(info->stream, "}");
}

static void print_unknown(disassemble_info *info, uint16_t insn)
{
    info->fprintf_func(info->stream, ".word 0x%04x", insn);
}

int myemulator_print_insn(bfd_vma address, disassemble_info *info)
{
    bfd_byte buffer[2];
    uint16_t insn;
    unsigned opcode;
    unsigned rd;
    unsigned rn;
    uint8_t operand;
    bool known = true;

    if (info->read_memory_func(address, buffer, sizeof(buffer), info) != 0) {
        info->memory_error_func(EIO, address, info);
        return -1;
    }
    insn = bfd_getl16(buffer);
    opcode = insn >> 12;
    rd = (insn >> 10) & 3;
    rn = (insn >> 8) & 3;
    operand = insn & 0xff;

    switch (opcode) {
    case 0x0:
        info->fprintf_func(info->stream, "ld %s,", data_regs[rd]);
        print_memory_operand(info, rn, operand);
        break;
    case 0x1:
        info->fprintf_func(info->stream, "li %s, #0x%02x",
                           data_regs[rd], operand);
        break;
    case 0x2:
        info->fprintf_func(info->stream, "st %s,", data_regs[rd]);
        print_memory_operand(info, rn, operand);
        break;
    case 0x3:
    case 0x4:
        info->fprintf_func(info->stream, "%s %s,%s,#0x%02x",
                           opcode == 3 ? "add" : "sub", data_regs[rd],
                           data_regs[rn], operand);
        break;
    case 0x5:
        info->fprintf_func(info->stream, "bl 0x%04x",
                           branch_target(address, operand));
        break;
    case 0x6: {
        static const char * const branches[] = {
            "beq", "bne", "blt", "bge", "bltu", "bgeu", "br"
        };
        unsigned condition = (insn >> 8) & 0xf;

        if (condition <= 6) {
            info->fprintf_func(info->stream, "%s 0x%04x",
                               branches[condition],
                               branch_target(address, operand));
        } else {
            known = false;
        }
        break;
    }
    case 0x7: {
        unsigned subop = (insn >> 8) & 0xf;

        switch (subop) {
        case 0x0: case 0x4: case 0x8: case 0xc:
            info->fprintf_func(info->stream, "ada %s,#%d",
                               address_regs[rd], sign8(operand));
            break;
        case 0x1:
            if ((insn & 3) != 0) {
                known = false;
                break;
            }
            info->fprintf_func(info->stream, "lda %s,%s,%s",
                               address_regs[(insn >> 6) & 3],
                               data_regs[(insn >> 4) & 3],
                               data_regs[(insn >> 2) & 3]);
            break;
        case 0x2:
            if ((insn & 3) != 0) {
                known = false;
                break;
            }
            info->fprintf_func(info->stream, "gta %s,%s,%s",
                               data_regs[(insn >> 4) & 3],
                               data_regs[(insn >> 2) & 3],
                               address_regs[(insn >> 6) & 3]);
            break;
        case 0x3: {
            unsigned dst = (insn >> 3) & 7;
            unsigned src = insn & 7;

            if ((insn & 0xc0) != 0 || dst > 5 || src > 5) {
                known = false;
                break;
            }
            info->fprintf_func(info->stream, "mva %s,%s",
                               wide_regs[dst], wide_regs[src]);
            break;
        }
        case 0x5:
            if ((insn & 0x3f) > 1) {
                known = false;
                break;
            }
            info->fprintf_func(info->stream, "%s %s",
                               (insn & 1) ? "jla" : "ja",
                               address_regs[(insn >> 6) & 3]);
            break;
        case 0x6:
            if ((insn & 3) > 1) {
                known = false;
                break;
            }
            info->fprintf_func(info->stream, "%s %s",
                               (insn & 1) ? "sf" : "gf",
                               data_regs[(insn >> 2) & 3]);
            break;
        case 0x7: {
            unsigned alu = (insn >> 4) & 0xf;

            if (alu > 6) {
                known = false;
                break;
            }
            info->fprintf_func(info->stream, "%s %s,%s",
                               alu_names[alu], data_regs[(insn >> 2) & 3],
                               data_regs[insn & 3]);
            break;
        }
        default:
            known = false;
            break;
        }
        break;
    }
    case 0x8:
        if (operand != 0) {
            known = false;
            break;
        }
        info->fprintf_func(info->stream, "cmp %s,%s",
                           data_regs[rd], data_regs[rn]);
        break;
    case 0x9: case 0xa: case 0xb:
        info->fprintf_func(info->stream, "%s %s,%s,#0x%02x",
                           opcode == 9 ? "and" : opcode == 0xa ? "or" : "xor",
                           data_regs[rd], data_regs[rn], operand);
        break;
    case 0xc: case 0xd:
        info->fprintf_func(info->stream, "%s %s,%s,#0x%02x",
                           opcode == 0xc ? "shl" : "shr", data_regs[rd],
                           data_regs[rn], operand);
        break;
    case 0xe:
        if (operand > 0x1f) {
            known = false;
            break;
        }
        print_push_pop(info, "push", operand);
        break;
    case 0xf:
        if (operand < 0x20) {
            print_push_pop(info, "pop", operand);
        } else if (insn == 0xf040) {
            info->fprintf_func(info->stream, "ret");
        } else if (insn == 0xf060) {
            info->fprintf_func(info->stream, "rti");
        } else if (insn == 0xf080) {
            info->fprintf_func(info->stream, "halt");
        } else {
            known = false;
        }
        break;
    default:
        known = false;
        break;
    }

    if (!known) {
        print_unknown(info, insn);
    }
    return 2;
}
