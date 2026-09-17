#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu/bitops.h"
#include "exec/exec-all.h"
#include "tcg/tcg-op.h"
#include "exec/translator.h"
#include "exec/translation-block.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"

#define HELPER_H "helper.h"
#include "exec/helper-info.c.inc"
#undef HELPER_H

#define DISAS_EXIT DISAS_TARGET_0

typedef struct DisasContext {
    DisasContextBase base;
    CPUMyEmulatorState *env;
} DisasContext;

static TCGv_i32 cpu_pc;
static TCGv_i32 cpu_sp;
static TCGv_i32 cpu_lr;
static TCGv_i32 cpu_r[4];
static TCGv_i32 cpu_a[4];
static TCGv_i32 cpu_s0;

void myemulator_cpu_tcg_init(void)
{
    static const char * const names[4] = { "r0", "r1", "r2", "r3" };
    static const char * const address_names[4] = { "a0", "a1", "a2", "a3" };
    int i;

    cpu_pc = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, pc), "pc");
    cpu_sp = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, sp), "sp");
    cpu_lr = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, lr), "lr");
    cpu_s0 = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, s0), "s0");
    for (i = 0; i < 4; i++) {
        cpu_r[i] = tcg_global_mem_new_i32(tcg_env,
                                          offsetof(CPUMyEmulatorState, r[i]),
                                          names[i]);
        cpu_a[i] = tcg_global_mem_new_i32(tcg_env,
                                          offsetof(CPUMyEmulatorState, a[i]),
                                          address_names[i]);
    }
}

static void gen_next_pc(DisasContext *ctx)
{
    tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next & 0xffff);
}

static void gen_pc_relative_branch(DisasContext *ctx, uint8_t imm,
                                   TCGCond condition, TCGv_i32 lhs,
                                   TCGv_i32 rhs)
{
    TCGLabel *taken = gen_new_label();
    TCGLabel *done = gen_new_label();
    int target = (ctx->base.pc_next + ((int8_t)imm * 2)) & 0xffff;

    tcg_gen_brcond_i32(condition, lhs, rhs, taken);
    gen_next_pc(ctx);
    tcg_gen_br(done);
    gen_set_label(taken);
    tcg_gen_movi_i32(cpu_pc, target);
    gen_set_label(done);
    ctx->base.is_jmp = DISAS_EXIT;
}

static void gen_sub_flags(TCGv_i32 lhs, TCGv_i32 rhs, TCGv_i32 result)
{
    TCGv_i32 tmp = tcg_temp_new_i32();
    TCGv_i32 result8 = tcg_temp_new_i32();

    tcg_gen_andi_i32(result8, result, 0xff);
    tcg_gen_setcondi_i32(TCG_COND_EQ, tmp, result8, 0);
    tcg_gen_deposit_i32(cpu_s0, cpu_s0, tmp, 0, 1);
    tcg_gen_andi_i32(tmp, result8, 0x80);
    tcg_gen_setcondi_i32(TCG_COND_NE, tmp, tmp, 0);
    tcg_gen_deposit_i32(cpu_s0, cpu_s0, tmp, 1, 1);
    tcg_gen_setcond_i32(TCG_COND_GEU, tmp, lhs, rhs);
    tcg_gen_deposit_i32(cpu_s0, cpu_s0, tmp, 2, 1);

    /* Signed subtraction overflow: (lhs ^ rhs) & (lhs ^ result) & 0x80. */
    tcg_gen_xor_i32(tmp, lhs, rhs);
    tcg_gen_xor_i32(result8, lhs, result8);
    tcg_gen_and_i32(tmp, tmp, result8);
    tcg_gen_andi_i32(tmp, tmp, 0x80);
    tcg_gen_setcondi_i32(TCG_COND_NE, tmp, tmp, 0);
    tcg_gen_deposit_i32(cpu_s0, cpu_s0, tmp, 3, 1);
}

static void gen_set_s0_bit(int bit, TCGv_i32 value)
{
    tcg_gen_deposit_i32(cpu_s0, cpu_s0, value, bit, 1);
}

static TCGv_i32 gen_s0_bit(int bit)
{
    TCGv_i32 value = tcg_temp_new_i32();

    tcg_gen_andi_i32(value, cpu_s0, 1U << bit);
    tcg_gen_shri_i32(value, value, bit);
    return value;
}

static void gen_push_register(uint8_t reg)
{
    if (reg == 4) {
        tcg_gen_subi_i32(cpu_sp, cpu_sp, 1);
        tcg_gen_qemu_st_i32(cpu_lr, cpu_sp, MMU_PHYS_IDX, MO_UB);
        tcg_gen_subi_i32(cpu_sp, cpu_sp, 1);
        TCGv_i32 high = tcg_temp_new_i32();
        tcg_gen_shri_i32(high, cpu_lr, 8);
        tcg_gen_qemu_st_i32(high, cpu_sp, MMU_PHYS_IDX, MO_UB);
    } else {
        tcg_gen_subi_i32(cpu_sp, cpu_sp, 1);
        tcg_gen_qemu_st_i32(cpu_r[reg], cpu_sp, MMU_PHYS_IDX, MO_UB);
    }
}

static void gen_pop_register(uint8_t reg)
{
    TCGv_i32 tmp = tcg_temp_new_i32();

    if (reg == 4) {
        TCGv_i32 low = tcg_temp_new_i32();
        tcg_gen_qemu_ld_i32(tmp, cpu_sp, MMU_PHYS_IDX, MO_UB);
        tcg_gen_addi_i32(cpu_sp, cpu_sp, 1);
        tcg_gen_qemu_ld_i32(low, cpu_sp, MMU_PHYS_IDX, MO_UB);
        tcg_gen_addi_i32(cpu_sp, cpu_sp, 1);
        tcg_gen_shli_i32(tmp, tmp, 8);
        tcg_gen_or_i32(cpu_lr, tmp, low);
    } else {
        tcg_gen_qemu_ld_i32(tmp, cpu_sp, MMU_PHYS_IDX, MO_UB);
        tcg_gen_mov_i32(cpu_r[reg], tmp);
        tcg_gen_addi_i32(cpu_sp, cpu_sp, 1);
    }
}

static void decode_and_translate(DisasContext *ctx)
{
    uint16_t insn = translator_ldub(ctx->env, &ctx->base, ctx->base.pc_next) |
                    (translator_ldub(ctx->env, &ctx->base,
                                     ctx->base.pc_next + 1) << 8);
    uint8_t opcode = extract32(insn, 12, 4);
    uint8_t regs = extract32(insn, 8, 4);
    uint8_t rd = extract32(regs, 2, 2);
    uint8_t rn = extract32(regs, 0, 2);
    uint8_t imm = extract32(insn, 0, 8);
    TCGv_i32 tmp;

    tcg_gen_insn_start(ctx->base.pc_next);
    ctx->base.pc_next = (ctx->base.pc_next + 2) & 0xffff;

    switch (opcode) {
    case 0x1: /* LI */
        tcg_gen_movi_i32(cpu_r[rd], imm);
        gen_next_pc(ctx);
        break;
    case 0x3: /* ADD immediate, matching the Python emulator path */
        {
        TCGv_i32 lhs = tcg_temp_new_i32();
        TCGv_i32 rhs = tcg_temp_new_i32();
        tmp = tcg_temp_new_i32();
        tcg_gen_andi_i32(lhs, cpu_r[rn], 0xff);
        tcg_gen_movi_i32(rhs, imm);
        tcg_gen_add_i32(tmp, lhs, rhs);
        TCGv_i32 flag = tcg_temp_new_i32();
        tcg_gen_setcondi_i32(TCG_COND_GTU, flag, tmp, 255);
        gen_set_s0_bit(2, flag);
        tcg_gen_andi_i32(cpu_r[rd], tmp, 0xff);
        tcg_gen_setcondi_i32(TCG_COND_EQ, flag, cpu_r[rd], 0);
        gen_set_s0_bit(0, flag);
        tcg_gen_shri_i32(tmp, cpu_r[rd], 7);
        gen_set_s0_bit(1, tmp);
        tcg_gen_xor_i32(tmp, lhs, rhs);
        tcg_gen_xor_i32(rhs, lhs, cpu_r[rd]);
        tcg_gen_and_i32(tmp, tmp, rhs);
        tcg_gen_andi_i32(tmp, tmp, 0x80);
        tcg_gen_setcondi_i32(TCG_COND_NE, flag, tmp, 0);
        gen_set_s0_bit(3, flag);
        gen_next_pc(ctx);
        }
        break;
    case 0x4: /* SUB immediate, with subtraction flags. */
        {
        TCGv_i32 lhs = tcg_temp_new_i32();
        TCGv_i32 rhs = tcg_temp_new_i32();
        tmp = tcg_temp_new_i32();
        tcg_gen_andi_i32(lhs, cpu_r[rn], 0xff);
        tcg_gen_movi_i32(rhs, imm);
        tcg_gen_sub_i32(tmp, lhs, rhs);
        gen_sub_flags(lhs, rhs, tmp);
        tcg_gen_andi_i32(cpu_r[rd], tmp, 0xff);
        gen_next_pc(ctx);
        }
        break;
    case 0x8: /* CMP register-to-register; result is discarded. */
        {
        TCGv_i32 lhs = tcg_temp_new_i32();
        TCGv_i32 rhs = tcg_temp_new_i32();
        tmp = tcg_temp_new_i32();
        tcg_gen_andi_i32(lhs, cpu_r[rd], 0xff);
        tcg_gen_andi_i32(rhs, cpu_r[rn], 0xff);
        tcg_gen_sub_i32(tmp, lhs, rhs);
        gen_sub_flags(lhs, rhs, tmp);
        gen_next_pc(ctx);
        }
        break;
    case 0x2: /* ST */
        tmp = tcg_temp_new_i32();
        TCGv_i32 address = tcg_temp_new_i32();
        tcg_gen_addi_i32(address, cpu_a[rn], (int8_t)imm);
        tcg_gen_andi_i32(address, address, 0xffff);
        tcg_gen_andi_i32(tmp, cpu_r[rd], 0xff);
        tcg_gen_qemu_st_i32(tmp, address, MMU_PHYS_IDX, MO_UB);
        gen_next_pc(ctx);
        break;
    case 0x0: /* LD */
        tmp = tcg_temp_new_i32();
        {
            TCGv_i32 load_address = tcg_temp_new_i32();
            tcg_gen_addi_i32(load_address, cpu_a[rn], (int8_t)imm);
            tcg_gen_andi_i32(load_address, load_address, 0xffff);
            tcg_gen_qemu_ld_i32(tmp, load_address, MMU_PHYS_IDX, MO_UB);
            tcg_gen_mov_i32(cpu_r[rd], tmp);
        }
        gen_next_pc(ctx);
        break;
    case 0x5: /* JAL, signed PC-relative displacement in instruction units. */
        tcg_gen_movi_i32(cpu_lr, ctx->base.pc_next);
        tcg_gen_movi_i32(cpu_pc,
                         (ctx->base.pc_next + ((int8_t)imm * 2)) & 0xffff);
        ctx->base.is_jmp = DISAS_EXIT;
        break;
    case 0x6: /* Conditional branch family. */
        switch (regs) {
        case 0x0: /* BEQ: ZF == 1 */
            gen_pc_relative_branch(ctx, imm, TCG_COND_NE, gen_s0_bit(0),
                                   tcg_constant_i32(0));
            break;
        case 0x1: /* BNE: ZF == 0 */
            gen_pc_relative_branch(ctx, imm, TCG_COND_EQ, gen_s0_bit(0),
                                   tcg_constant_i32(0));
            break;
        case 0x2: /* BLT: NF != OF */
            gen_pc_relative_branch(ctx, imm, TCG_COND_NE, gen_s0_bit(1),
                                   gen_s0_bit(3));
            break;
        case 0x3: /* BGE: NF == OF */
            gen_pc_relative_branch(ctx, imm, TCG_COND_EQ, gen_s0_bit(1),
                                   gen_s0_bit(3));
            break;
        case 0x4: /* BLTU: CF == 0 */
            gen_pc_relative_branch(ctx, imm, TCG_COND_EQ, gen_s0_bit(2),
                                   tcg_constant_i32(0));
            break;
        case 0x5: /* BGEU: CF == 1 */
            gen_pc_relative_branch(ctx, imm, TCG_COND_NE, gen_s0_bit(2),
                                   tcg_constant_i32(0));
            break;
        case 0x6: /* BR: unconditional relative branch. */
            tcg_gen_movi_i32(cpu_pc,
                             (ctx->base.pc_next + ((int8_t)imm * 2)) & 0xffff);
            ctx->base.is_jmp = DISAS_EXIT;
            break;
        default:
            tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
            gen_helper_illegal(tcg_env);
            ctx->base.is_jmp = DISAS_NORETURN;
            break;
        }
        break;
    case 0xe: /* PUSH, ascending R0..R3 then LR. */
        for (int reg = 0; reg < 5; reg++) {
            if (imm & (1 << reg)) {
                gen_push_register(reg);
            }
        }
        gen_next_pc(ctx);
        break;
    case 0xf: /* System instructions and POP. */
        if ((insn & 0x0fff) == 0x080) {
            gen_next_pc(ctx);
            gen_helper_halt(tcg_env);
            ctx->base.is_jmp = DISAS_NORETURN;
        } else if ((insn & 0x0fff) == 0x040) { /* RET. */
            tcg_gen_mov_i32(cpu_pc, cpu_lr);
            ctx->base.is_jmp = DISAS_EXIT;
        } else if ((insn & 0x0fff) == 0x060) { /* RTI. */
            TCGv_i32 low = tcg_temp_new_i32();

            tmp = tcg_temp_new_i32();
            tcg_gen_qemu_ld_i32(tmp, cpu_sp, MMU_PHYS_IDX, MO_UB);
            tcg_gen_addi_i32(cpu_sp, cpu_sp, 1);
            tcg_gen_andi_i32(cpu_s0, tmp, 0xff);
            tcg_gen_qemu_ld_i32(tmp, cpu_sp, MMU_PHYS_IDX, MO_UB);
            tcg_gen_addi_i32(cpu_sp, cpu_sp, 1);
            tcg_gen_shli_i32(tmp, tmp, 8);
            tcg_gen_qemu_ld_i32(low, cpu_sp, MMU_PHYS_IDX, MO_UB);
            tcg_gen_addi_i32(cpu_sp, cpu_sp, 1);
            tcg_gen_or_i32(cpu_pc, tmp, low);
            ctx->base.is_jmp = DISAS_EXIT;
        } else if (imm < 0x20) { /* POP, reverse LR..R0. */
            for (int reg = 4; reg >= 0; reg--) {
                if (imm & (1 << reg)) {
                    gen_pop_register(reg);
                }
            }
            gen_next_pc(ctx);
        } else {
            tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
            gen_helper_illegal(tcg_env);
            ctx->base.is_jmp = DISAS_NORETURN;
        }
        break;
    case 0x7: /* Address-register operation family. */
        switch (extract32(insn, 8, 4)) {
        case 0x0: case 0x4: case 0x8: case 0xc: /* ADA */
            tcg_gen_addi_i32(cpu_a[extract32(insn, 10, 2)],
                             cpu_a[extract32(insn, 10, 2)], (int8_t)imm);
            tcg_gen_andi_i32(cpu_a[extract32(insn, 10, 2)],
                             cpu_a[extract32(insn, 10, 2)], 0xffff);
            gen_next_pc(ctx);
            break;
        case 0x1: /* LDA: 0x71 An Rx Ry 00 */
            tmp = tcg_temp_new_i32();
            tcg_gen_shli_i32(tmp, cpu_r[extract32(insn, 4, 2)], 8);
            tcg_gen_or_i32(cpu_a[extract32(insn, 6, 2)], tmp,
                           cpu_r[extract32(insn, 2, 2)]);
            gen_next_pc(ctx);
            break;
        case 0x2: /* GTA: 0x72 An Rx Ry 00 */
            tcg_gen_shri_i32(cpu_r[extract32(insn, 4, 2)],
                             cpu_a[extract32(insn, 6, 2)], 8);
            tcg_gen_andi_i32(cpu_r[extract32(insn, 4, 2)],
                             cpu_r[extract32(insn, 4, 2)], 0xff);
            tcg_gen_andi_i32(cpu_r[extract32(insn, 2, 2)],
                             cpu_a[extract32(insn, 6, 2)], 0xff);
            gen_next_pc(ctx);
            break;
        case 0x3: /* MVA: 0x73 Ad As 00 */
            {
                unsigned dst = extract32(insn, 3, 3);
                unsigned src = extract32(insn, 0, 3);

                if ((insn & 0xc0) != 0 || dst > 5 || src > 5) {
                    tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
                    gen_helper_illegal(tcg_env);
                    ctx->base.is_jmp = DISAS_NORETURN;
                    break;
                }
                TCGv_i32 mva_regs[6] = {
                    cpu_a[0], cpu_a[1], cpu_a[2], cpu_a[3], cpu_lr, cpu_sp,
                };
                tcg_gen_mov_i32(mva_regs[dst], mva_regs[src]);
                gen_next_pc(ctx);
            }
            break;
        case 0x6: /* GF/SF: 0x7600 | (Rn << 2) | op. */
            if ((insn & 0x3) == 0) {
                tcg_gen_andi_i32(cpu_r[extract32(insn, 2, 2)], cpu_s0,
                                 0xff);
                gen_next_pc(ctx);
            } else if ((insn & 0x3) == 1) {
                tcg_gen_andi_i32(cpu_s0, cpu_r[extract32(insn, 2, 2)],
                                 0xff);
                gen_next_pc(ctx);
            } else {
                tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
                gen_helper_illegal(tcg_env);
                ctx->base.is_jmp = DISAS_NORETURN;
            }
            break;
        default:
            tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
            gen_helper_illegal(tcg_env);
            ctx->base.is_jmp = DISAS_NORETURN;
            break;
        }
        break;
    default:
        tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
        gen_helper_illegal(tcg_env);
        ctx->base.is_jmp = DISAS_NORETURN;
        break;
    }
}

static void myemulator_tr_init_disas_context(DisasContextBase *dcbase,
                                             CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    ctx->env = cpu_env(cs);
}

static void myemulator_tr_tb_start(DisasContextBase *dcbase, CPUState *cs)
{
}

static void myemulator_tr_insn_start(DisasContextBase *dcbase, CPUState *cs)
{
}

static void myemulator_tr_translate_insn(DisasContextBase *dcbase,
                                         CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    decode_and_translate(ctx);
    if (ctx->base.is_jmp == DISAS_NEXT) {
        /* External IRQs are accepted only between architecturally complete
         * instructions, so keep each translation block to one instruction. */
        ctx->base.is_jmp = DISAS_EXIT;
    }
}

static void myemulator_tr_tb_stop(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    switch (ctx->base.is_jmp) {
    case DISAS_NEXT:
    case DISAS_TOO_MANY:
        tcg_gen_goto_tb(0);
        tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
        tcg_gen_exit_tb(ctx->base.tb, 0);
        break;
    case DISAS_EXIT:
        tcg_gen_exit_tb(NULL, 0);
        break;
    case DISAS_NORETURN:
        break;
    default:
        g_assert_not_reached();
    }
}

static const TranslatorOps myemulator_tr_ops = {
    .init_disas_context = myemulator_tr_init_disas_context,
    .tb_start = myemulator_tr_tb_start,
    .insn_start = myemulator_tr_insn_start,
    .translate_insn = myemulator_tr_translate_insn,
    .tb_stop = myemulator_tr_tb_stop,
};

void gen_intermediate_code(CPUState *cs, TranslationBlock *tb,
                           int *max_insns, vaddr pc, void *host_pc)
{
    DisasContext dc = { };

    translator_loop(cs, tb, max_insns, pc, host_pc, &myemulator_tr_ops,
                    &dc.base);
}
