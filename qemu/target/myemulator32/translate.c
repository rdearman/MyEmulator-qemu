#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu/bitops.h"
#include "exec/exec-all.h"
#include "exec/translator.h"
#include "exec/target_page.h"
#include "exec/translation-block.h"
#include "tcg/tcg-op.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"

#define HELPER_H "helper.h"
#include "exec/helper-info.c.inc"
#undef HELPER_H

#define DISAS_EXIT DISAS_TARGET_0

typedef struct DisasContext {
    DisasContextBase base;
    CPUMyEmulator32State *env;
} DisasContext;

static TCGv_i32 cpu_pc;
static TCGv_i32 cpu_sr;
static TCGv_i32 cpu_r[16];

static TCGv_i32 gen_reg(unsigned n)
{
    return n == 0 ? tcg_constant_i32(0) : cpu_r[n];
}

static void gen_write_reg(unsigned n, TCGv_i32 value)
{
    if (n != 0) {
        tcg_gen_mov_i32(cpu_r[n], value);
    }
}

static void gen_next_pc(DisasContext *ctx)
{
    tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
}

static void gen_sr_bit(unsigned bit, TCGv_i32 value)
{
    TCGv_i32 tmp = tcg_temp_new_i32();

    tcg_gen_andi_i32(cpu_sr, cpu_sr, ~(1u << bit));
    tcg_gen_andi_i32(tmp, value, 1);
    tcg_gen_shli_i32(tmp, tmp, bit);
    tcg_gen_or_i32(cpu_sr, cpu_sr, tmp);
}

static void gen_add_flags(TCGv_i32 lhs, TCGv_i32 rhs, TCGv_i32 result)
{
    TCGv_i32 tmp = tcg_temp_new_i32();

    tcg_gen_setcond_i32(TCG_COND_LTU, tmp, result, lhs);
    gen_sr_bit(0, tmp);
    /* OF = ((~(a ^ b)) & (a ^ r)) >> 31. */
    tcg_gen_xor_i32(tmp, lhs, rhs);
    tcg_gen_not_i32(tmp, tmp);
    TCGv_i32 other = tcg_temp_new_i32();
    tcg_gen_xor_i32(other, lhs, result);
    tcg_gen_and_i32(tmp, tmp, other);
    tcg_gen_shri_i32(tmp, tmp, 31);
    gen_sr_bit(1, tmp);
}

static void gen_sub_flags(TCGv_i32 lhs, TCGv_i32 rhs, TCGv_i32 result)
{
    TCGv_i32 tmp = tcg_temp_new_i32();

    tcg_gen_setcond_i32(TCG_COND_LTU, tmp, lhs, rhs);
    gen_sr_bit(0, tmp);
    /* OF = ((a ^ b) & (a ^ r)) >> 31. */
    tcg_gen_xor_i32(tmp, lhs, rhs);
    TCGv_i32 other = tcg_temp_new_i32();
    tcg_gen_xor_i32(other, lhs, result);
    tcg_gen_and_i32(tmp, tmp, other);
    tcg_gen_shri_i32(tmp, tmp, 31);
    gen_sr_bit(1, tmp);
}

static void gen_invalid(DisasContext *ctx, uint32_t insn)
{
    gen_next_pc(ctx);
    gen_helper_exception(tcg_env, tcg_constant_i32(MYEMU32_VECTOR_ILLEGAL),
                         tcg_constant_i32(ctx->base.pc_next - 4),
                         tcg_constant_i32(insn));
    ctx->base.is_jmp = DISAS_NORETURN;
}

static void gen_checked_indirect(DisasContext *ctx, TCGv_i32 target,
                                bool link)
{
    TCGLabel *aligned = gen_new_label();
    TCGv_i32 bit = tcg_temp_new_i32();
    uint32_t fault_pc = ctx->base.pc_next - 4;

    tcg_gen_andi_i32(bit, target, 3);
    tcg_gen_brcondi_i32(TCG_COND_EQ, bit, 0, aligned);
    gen_helper_exception(tcg_env, tcg_constant_i32(MYEMU32_VECTOR_INSN_ALIGN),
                         tcg_constant_i32(fault_pc), target);
    ctx->base.is_jmp = DISAS_NORETURN;
    gen_set_label(aligned);
    if (link) {
        tcg_gen_movi_i32(cpu_r[14], ctx->base.pc_next);
    }
    tcg_gen_mov_i32(cpu_pc, target);
    ctx->base.is_jmp = DISAS_EXIT;
}

static void gen_checked_data(DisasContext *ctx, TCGv_i32 address)
{
    TCGLabel *aligned = gen_new_label();
    TCGv_i32 bit = tcg_temp_new_i32();

    tcg_gen_andi_i32(bit, address, 3);
    tcg_gen_brcondi_i32(TCG_COND_EQ, bit, 0, aligned);
    gen_helper_exception(tcg_env, tcg_constant_i32(MYEMU32_VECTOR_DATA_ALIGN),
                         tcg_constant_i32(ctx->base.pc_next - 4), address);
    gen_set_label(aligned);
}

static void gen_branch(DisasContext *ctx, TCGCond cond, unsigned ra,
                       unsigned rb, int32_t displacement)
{
    TCGLabel *taken = gen_new_label();
    TCGLabel *done = gen_new_label();
    uint32_t target = ctx->base.pc_next + (displacement << 2);

    tcg_gen_brcond_i32(cond, gen_reg(ra), gen_reg(rb), taken);
    gen_next_pc(ctx);
    tcg_gen_br(done);
    gen_set_label(taken);
    tcg_gen_movi_i32(cpu_pc, target);
    gen_set_label(done);
    ctx->base.is_jmp = DISAS_EXIT;
}

static int32_t sx(uint32_t value, unsigned bits)
{
    uint32_t sign = 1u << (bits - 1);
    return (int32_t)((value ^ sign) - sign);
}

static void decode_and_translate(DisasContext *ctx)
{
    uint32_t pc = ctx->base.pc_next;
    uint32_t insn = translator_ldl(ctx->env, &ctx->base, pc);
    unsigned op = extract32(insn, 26, 6);
    unsigned rd = extract32(insn, 21, 5);
    unsigned ra = extract32(insn, 16, 5);
    unsigned rb = extract32(insn, 11, 5);
    unsigned fn = extract32(insn, 6, 5);
    unsigned sub = extract32(insn, 12, 4);
    unsigned imm12 = extract32(insn, 0, 12);
    TCGv_i32 tmp, lhs, rhs, result, address;

    tcg_gen_insn_start(pc);
    ctx->base.pc_next = pc + 4;

    switch (op) {
    case 0: /* R */
        if (rd > 15 || ra > 15 || rb > 15) { gen_invalid(ctx, insn); return; }
        if (extract32(insn, 0, 6) != 0 || fn > 25) {
            gen_invalid(ctx, insn); return;
        }
        lhs = gen_reg(ra); rhs = gen_reg(rb); result = tcg_temp_new_i32();
        switch (fn) {
        case 0: tcg_gen_add_i32(result, lhs, rhs); gen_add_flags(lhs, rhs, result); break;
        case 2: tcg_gen_sub_i32(result, lhs, rhs); gen_sub_flags(lhs, rhs, result); break;
        case 11: tcg_gen_and_i32(result, lhs, rhs); break;
        case 12: tcg_gen_or_i32(result, lhs, rhs); break;
        case 13: tcg_gen_xor_i32(result, lhs, rhs); break;
        case 14:
            if (rb != 0) { gen_invalid(ctx, insn); return; }
            tcg_gen_not_i32(result, lhs); break;
        default:
            gen_invalid(ctx, insn); return;
        }
        gen_write_reg(rd, result);
        gen_next_pc(ctx);
        return;
    case 1: /* ALU immediate */
        if (rd > 15 || ra > 15) { gen_invalid(ctx, insn); return; }
        lhs = gen_reg(ra); result = tcg_temp_new_i32();
        switch (sub) {
        case 0:
            tcg_gen_addi_i32(result, lhs, sx(imm12, 12));
            gen_add_flags(lhs, tcg_constant_i32(sx(imm12, 12)), result);
            break;
        case 1:
            tcg_gen_subi_i32(result, lhs, sx(imm12, 12));
            gen_sub_flags(lhs, tcg_constant_i32(sx(imm12, 12)), result);
            break;
        case 5: case 6: case 7:
            if ((imm12 & ~31u) != 0) { gen_invalid(ctx, insn); return; }
            if (sub == 5) tcg_gen_shli_i32(result, lhs, imm12 & 31);
            if (sub == 6) tcg_gen_shri_i32(result, lhs, imm12 & 31);
            if (sub == 7) tcg_gen_sari_i32(result, lhs, imm12 & 31);
            break;
        default:
            gen_invalid(ctx, insn); return;
        }
        gen_write_reg(rd, result);
        gen_next_pc(ctx);
        return;
    case 2: /* LOAD */
        if (rd > 15 || ra > 15) { gen_invalid(ctx, insn); return; }
        if (extract32(insn, 13, 3) != 4) { gen_invalid(ctx, insn); return; }
        address = tcg_temp_new_i32();
        tcg_gen_addi_i32(address, gen_reg(ra), sx(extract32(insn, 0, 13), 13));
        gen_checked_data(ctx, address);
        tmp = tcg_temp_new_i32();
        tcg_gen_qemu_ld_i32(tmp, address, MMU_PHYS_IDX, MO_LEUL);
        gen_write_reg(rd, tmp);
        gen_next_pc(ctx);
        return;
    case 3: /* STORE */
        if (rd > 15 || ra > 15) { gen_invalid(ctx, insn); return; }
        if (extract32(insn, 13, 3) != 2) { gen_invalid(ctx, insn); return; }
        address = tcg_temp_new_i32();
        tcg_gen_addi_i32(address, gen_reg(ra), sx(extract32(insn, 0, 13), 13));
        gen_checked_data(ctx, address);
        tcg_gen_qemu_st_i32(gen_reg(rd), address, MMU_PHYS_IDX, MO_LEUL);
        gen_next_pc(ctx);
        return;
    case 4: /* branches */
        if (extract32(insn, 23, 3) > 1 || extract32(insn, 18, 5) > 15 ||
            extract32(insn, 13, 5) > 15) { gen_invalid(ctx, insn); return; }
        gen_branch(ctx, extract32(insn, 23, 3) == 0 ? TCG_COND_EQ : TCG_COND_NE,
                   extract32(insn, 18, 5), extract32(insn, 13, 5),
                   sx(extract32(insn, 0, 13), 13));
        return;
    case 5: /* J */
        tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next + (sx(extract32(insn, 0, 26), 26) << 2));
        ctx->base.is_jmp = DISAS_EXIT;
        return;
    case 6: /* JAL */
        tcg_gen_movi_i32(cpu_r[14], ctx->base.pc_next);
        tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next + (sx(extract32(insn, 0, 26), 26) << 2));
        ctx->base.is_jmp = DISAS_EXIT;
        return;
    case 7: /* JR/JALR */
        if ((insn & 0xfffffu) != 0 || extract32(insn, 25, 1) > 1 ||
            extract32(insn, 20, 5) > 15) {
            gen_invalid(ctx, insn); return;
        }
        gen_checked_indirect(ctx, gen_reg(extract32(insn, 20, 5)),
                             extract32(insn, 25, 1));
        return;
    case 8: /* LUI */
        if (rd > 15) { gen_invalid(ctx, insn); return; }
        if ((insn & 1) != 0) { gen_invalid(ctx, insn); return; }
        tcg_gen_movi_i32(result = tcg_temp_new_i32(), extract32(insn, 1, 20) << 12);
        gen_write_reg(rd, result);
        gen_next_pc(ctx);
        return;
    case 9: /* SYSTEM */
        {
            unsigned sysop = extract32(insn, 22, 4);
            unsigned sysreg = extract32(insn, 11, 6);
            unsigned reg = extract32(insn, 17, 5);
            if (extract32(insn, 0, 11) != 0 || sysreg > 5) {
                gen_invalid(ctx, insn); return;
            }
            if (sysop == 0) {
                tmp = tcg_temp_new_i32();
                gen_helper_mfsr(tmp, tcg_env, tcg_constant_i32(sysreg),
                                tcg_constant_i32(pc));
                gen_write_reg(reg, tmp); gen_next_pc(ctx); return;
            }
            if (sysop == 1) {
                gen_helper_mtsr(tcg_env, tcg_constant_i32(sysreg), gen_reg(reg),
                                tcg_constant_i32(pc));
                gen_next_pc(ctx); return;
            }
            if (sysreg != 0 || reg != 0) { gen_invalid(ctx, insn); return; }
            if (sysop == 2) {
                gen_helper_rfe(tcg_env, tcg_constant_i32(pc));
                ctx->base.is_jmp = DISAS_EXIT; return;
            }
            if (sysop == 3) {
                gen_next_pc(ctx); gen_helper_halt(tcg_env);
                ctx->base.is_jmp = DISAS_NORETURN; return;
            }
            gen_invalid(ctx, insn); return;
        }
    default:
        gen_invalid(ctx, insn); return;
    }
}

void myemulator32_cpu_tcg_init(void)
{
    for (int i = 0; i < 16; i++) {
        char *name = g_strdup_printf("r%d", i);
        cpu_r[i] = tcg_global_mem_new_i32(tcg_env,
                                          offsetof(CPUMyEmulator32State, r[i]),
                                          name);
        g_free(name);
    }
    cpu_pc = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulator32State, pc), "pc");
    cpu_sr = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulator32State, sr), "sr");
}

static void myemulator32_init_disas(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    ctx->env = cpu_env(cs);
}

static void myemulator32_tb_start(DisasContextBase *dcbase, CPUState *cs)
{
    /* System-register writes, RFE, and exception entry update architectural
     * state from C helpers.  Refresh the TCG globals at every TB boundary so
     * banked SP and SR changes are observed by the next instruction. */
    tcg_gen_ld_i32(cpu_pc, tcg_env,
                   offsetof(CPUMyEmulator32State, pc));
    tcg_gen_ld_i32(cpu_sr, tcg_env,
                   offsetof(CPUMyEmulator32State, sr));
    tcg_gen_ld_i32(cpu_r[13], tcg_env,
                   offsetof(CPUMyEmulator32State, r[13]));
}

static void myemulator32_insn_start(DisasContextBase *dcbase, CPUState *cs)
{
}

static void myemulator32_translate_insn(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    decode_and_translate(ctx);
    if (ctx->base.is_jmp == DISAS_NEXT) ctx->base.is_jmp = DISAS_EXIT;
}

static void myemulator32_tb_stop(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    switch (ctx->base.is_jmp) {
    case DISAS_EXIT: tcg_gen_exit_tb(NULL, 0); break;
    case DISAS_NORETURN: break;
    default: g_assert_not_reached();
    }
}

static const TranslatorOps myemulator32_ops = {
    .init_disas_context = myemulator32_init_disas,
    .tb_start = myemulator32_tb_start,
    .insn_start = myemulator32_insn_start,
    .translate_insn = myemulator32_translate_insn,
    .tb_stop = myemulator32_tb_stop,
};

void gen_intermediate_code(CPUState *cs, TranslationBlock *tb,
                           int *max_insns, vaddr pc, void *host_pc)
{
    DisasContext dc = { };
    translator_loop(cs, tb, max_insns, pc, host_pc, &myemulator32_ops,
                    &dc.base);
}
