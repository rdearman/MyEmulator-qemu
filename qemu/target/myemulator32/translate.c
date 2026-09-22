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

static int myemu32_mmu_idx(const DisasContext *ctx)
{
    return (ctx->base.tb->flags & MYEMU32_SR_S) ?
           MMU_SUPERVISOR_IDX : MMU_USER_IDX;
}

static TCGv_i32 gen_reg(unsigned n)
{
    return n == 0 ? tcg_constant_i32(0) : cpu_r[n];
}

static void gen_write_reg(unsigned n, TCGv_i32 value)
{
    if (n == 0) {
        return;
    }
    tcg_gen_mov_i32(cpu_r[n], value);

    /* R13 is the visible view of the active banked stack pointer.  Keep
     * the backing bank synchronized for ordinary instruction writes too;
     * otherwise a task switch or an interrupt can leave env->ssp/env->usp
     * referring to a different stack than the visible R13. */
    if (n == 13) {
        TCGv_i32 sr_mode = tcg_temp_new_i32();
        TCGv_i32 old_ssp = tcg_temp_new_i32();
        TCGv_i32 old_usp = tcg_temp_new_i32();

        tcg_gen_andi_i32(sr_mode, cpu_sr, MYEMU32_SR_S);
        tcg_gen_ld_i32(old_ssp, tcg_env,
                       offsetof(CPUMyEmulator32State, ssp));
        tcg_gen_ld_i32(old_usp, tcg_env,
                       offsetof(CPUMyEmulator32State, usp));
        tcg_gen_movcond_i32(TCG_COND_NE, old_ssp, sr_mode,
                            tcg_constant_i32(0), value, old_ssp);
        tcg_gen_movcond_i32(TCG_COND_EQ, old_usp, sr_mode,
                            tcg_constant_i32(0), value, old_usp);
        tcg_gen_st_i32(old_ssp, tcg_env,
                       offsetof(CPUMyEmulator32State, ssp));
        tcg_gen_st_i32(old_usp, tcg_env,
                       offsetof(CPUMyEmulator32State, usp));
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

static void gen_checked_data(DisasContext *ctx, TCGv_i32 address,
                             unsigned alignment)
{
    TCGLabel *aligned = gen_new_label();
    TCGv_i32 bit = tcg_temp_new_i32();

    tcg_gen_andi_i32(bit, address, alignment - 1);
    tcg_gen_brcondi_i32(TCG_COND_EQ, bit, 0, aligned);
    gen_helper_exception(tcg_env, tcg_constant_i32(MYEMU32_VECTOR_DATA_ALIGN),
                         tcg_constant_i32(ctx->base.pc_next - 4), address);
    gen_set_label(aligned);
}

/* The banked stack pointer must be visible in CPUState before a memory
 * operation can leave the translation block through an MMU fault. */
static void gen_materialize_fault_state(void)
{
    tcg_gen_st_i32(cpu_r[13], tcg_env,
                   offsetof(CPUMyEmulator32State, r[13]));
}

static void gen_branch(DisasContext *ctx, TCGCond cond, unsigned ra,
                       unsigned rb, int32_t displacement)
{
    TCGLabel *taken = gen_new_label();
    TCGLabel *done = gen_new_label();
    uint32_t target = ctx->base.pc_next + (uint32_t)(displacement * 4);

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
        case 1:
            tmp = tcg_temp_new_i32();
            tcg_gen_andi_i32(tmp, cpu_sr, MYEMU32_SR_CF);
            tcg_gen_andi_i32(tmp, tmp, 1);
            gen_helper_adc(result, tcg_env, lhs, rhs, tmp);
            tcg_gen_ld_i32(cpu_sr, tcg_env,
                           offsetof(CPUMyEmulator32State, sr));
            break;
        case 2: tcg_gen_sub_i32(result, lhs, rhs); gen_sub_flags(lhs, rhs, result); break;
        case 3:
            tmp = tcg_temp_new_i32();
            tcg_gen_andi_i32(tmp, cpu_sr, MYEMU32_SR_CF);
            tcg_gen_andi_i32(tmp, tmp, 1);
            gen_helper_sbc(result, tcg_env, lhs, rhs, tmp);
            tcg_gen_ld_i32(cpu_sr, tcg_env,
                           offsetof(CPUMyEmulator32State, sr));
            break;
        case 4:
            {
                TCGv_i32 high = tcg_temp_new_i32();
                tcg_gen_muls2_i32(result, high, lhs, rhs);
                gen_write_reg(rd, result);
            }
            gen_next_pc(ctx);
            return;
        case 5:
            {
                TCGv_i32 high = tcg_temp_new_i32();
                tcg_gen_muls2_i32(result, high, lhs, rhs);
                gen_write_reg(rd, high);
                gen_next_pc(ctx);
                return;
            }
        case 6:
            {
                TCGv_i32 high = tcg_temp_new_i32();
                tcg_gen_mulu2_i32(result, high, lhs, rhs);
                gen_write_reg(rd, high);
                gen_next_pc(ctx);
                return;
            }
        case 7:
            gen_helper_div(result, tcg_env, lhs, rhs,
                           tcg_constant_i32(ctx->base.pc_next - 4));
            gen_write_reg(rd, result);
            gen_next_pc(ctx);
            return;
        case 8:
            gen_helper_divu(result, tcg_env, lhs, rhs,
                            tcg_constant_i32(ctx->base.pc_next - 4));
            gen_write_reg(rd, result);
            gen_next_pc(ctx);
            return;
        case 9:
            gen_helper_rem(result, tcg_env, lhs, rhs,
                           tcg_constant_i32(ctx->base.pc_next - 4));
            gen_write_reg(rd, result);
            gen_next_pc(ctx);
            return;
        case 10:
            gen_helper_remu(result, tcg_env, lhs, rhs,
                            tcg_constant_i32(ctx->base.pc_next - 4));
            gen_write_reg(rd, result);
            gen_next_pc(ctx);
            return;
        case 11: tcg_gen_and_i32(result, lhs, rhs); break;
        case 12: tcg_gen_or_i32(result, lhs, rhs); break;
        case 13: tcg_gen_xor_i32(result, lhs, rhs); break;
        case 14:
            if (rb != 0) { gen_invalid(ctx, insn); return; }
            tcg_gen_not_i32(result, lhs); break;
        case 15: case 16: case 17: case 18: case 19:
            tmp = tcg_temp_new_i32();
            tcg_gen_andi_i32(tmp, rhs, 31);
            if (fn == 15) tcg_gen_shl_i32(result, lhs, tmp);
            if (fn == 16) tcg_gen_shr_i32(result, lhs, tmp);
            if (fn == 17) tcg_gen_sar_i32(result, lhs, tmp);
            if (fn == 18) tcg_gen_rotl_i32(result, lhs, tmp);
            if (fn == 19) tcg_gen_rotr_i32(result, lhs, tmp);
            break;
        case 20: tcg_gen_setcond_i32(TCG_COND_EQ, result, lhs, rhs); break;
        case 21: tcg_gen_setcond_i32(TCG_COND_NE, result, lhs, rhs); break;
        case 22: tcg_gen_setcond_i32(TCG_COND_LT, result, lhs, rhs); break;
        case 23: tcg_gen_setcond_i32(TCG_COND_GE, result, lhs, rhs); break;
        case 24: tcg_gen_setcond_i32(TCG_COND_LTU, result, lhs, rhs); break;
        case 25: tcg_gen_setcond_i32(TCG_COND_GEU, result, lhs, rhs); break;
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
        case 2: tcg_gen_andi_i32(result, lhs, imm12); break;
        case 3: tcg_gen_ori_i32(result, lhs, imm12); break;
        case 4: tcg_gen_xori_i32(result, lhs, imm12); break;
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
        switch (extract32(insn, 13, 3)) {
        case 0: case 1: case 2: case 3: case 4: break;
        default: gen_invalid(ctx, insn); return;
        }
        unsigned load_size = extract32(insn, 13, 3);
        address = tcg_temp_new_i32();
        tcg_gen_addi_i32(address, gen_reg(ra), sx(extract32(insn, 0, 13), 13));
        gen_materialize_fault_state();
        gen_checked_data(ctx, address, load_size == 4 ? 4 :
                        (load_size >= 2 ? 2 : 1));
        tmp = tcg_temp_new_i32();
        switch (load_size) {
        case 0: tcg_gen_qemu_ld_i32(tmp, address, myemu32_mmu_idx(ctx), MO_SB); break;
        case 1: tcg_gen_qemu_ld_i32(tmp, address, myemu32_mmu_idx(ctx), MO_UB); break;
        case 2: tcg_gen_qemu_ld_i32(tmp, address, myemu32_mmu_idx(ctx), MO_LESW); break;
        case 3: tcg_gen_qemu_ld_i32(tmp, address, myemu32_mmu_idx(ctx), MO_LEUW); break;
        default: tcg_gen_qemu_ld_i32(tmp, address, myemu32_mmu_idx(ctx), MO_LEUL); break;
        }
        gen_write_reg(rd, tmp);
        gen_next_pc(ctx);
        return;
    case 3: /* STORE */
        if (rd > 15 || ra > 15) { gen_invalid(ctx, insn); return; }
        unsigned store_size = extract32(insn, 13, 3);
        if (store_size > 2) { gen_invalid(ctx, insn); return; }
        address = tcg_temp_new_i32();
        tcg_gen_addi_i32(address, gen_reg(ra), sx(extract32(insn, 0, 13), 13));
        gen_materialize_fault_state();
        gen_checked_data(ctx, address, store_size == 0 ? 1 :
                        (store_size == 1 ? 2 : 4));
        switch (store_size) {
        case 0: tcg_gen_qemu_st_i32(gen_reg(rd), address, myemu32_mmu_idx(ctx), MO_8); break;
        case 1: tcg_gen_qemu_st_i32(gen_reg(rd), address, myemu32_mmu_idx(ctx), MO_LEUW); break;
        default: tcg_gen_qemu_st_i32(gen_reg(rd), address, myemu32_mmu_idx(ctx), MO_LEUL); break;
        }
        gen_next_pc(ctx);
        return;
    case 4: /* branches */
        if (extract32(insn, 23, 3) > 5 || extract32(insn, 18, 5) > 15 ||
            extract32(insn, 13, 5) > 15) { gen_invalid(ctx, insn); return; }
        TCGCond branch_cond;
        switch (extract32(insn, 23, 3)) {
        case 0: branch_cond = TCG_COND_EQ; break;
        case 1: branch_cond = TCG_COND_NE; break;
        case 2: branch_cond = TCG_COND_LT; break;
        case 3: branch_cond = TCG_COND_GE; break;
        case 4: branch_cond = TCG_COND_LTU; break;
        default: branch_cond = TCG_COND_GEU; break;
        }
        gen_branch(ctx, branch_cond,
                   extract32(insn, 18, 5), extract32(insn, 13, 5),
                   sx(extract32(insn, 0, 13), 13));
        return;
    case 5: /* J */
        tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next +
                         (uint32_t)(sx(extract32(insn, 0, 26), 26) * 4));
        ctx->base.is_jmp = DISAS_EXIT;
        return;
    case 6: /* JAL */
        tcg_gen_movi_i32(cpu_r[14], ctx->base.pc_next);
        tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next +
                         (uint32_t)(sx(extract32(insn, 0, 26), 26) * 4));
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
            if (extract32(insn, 0, 11) != 0 || sysreg > 11) {
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
                gen_next_pc(ctx);
                ctx->base.is_jmp = DISAS_EXIT;
                return;
            }
            if (sysreg != 0 || reg != 0) { gen_invalid(ctx, insn); return; }
            if (sysop == 2) {
                /* The exception entry assembly restores the visible R13/R15
                 * immediately before RFE.  Materialize both globals before
                 * the helper changes the address-space/privilege state, so
                 * the next TB cannot inherit stale CPUState values. */
                tcg_gen_st_i32(cpu_r[13], tcg_env,
                               offsetof(CPUMyEmulator32State, r[13]));
                tcg_gen_st_i32(cpu_r[15], tcg_env,
                               offsetof(CPUMyEmulator32State, r[15]));
                gen_helper_rfe(tcg_env, tcg_constant_i32(pc));
                ctx->base.is_jmp = DISAS_EXIT; return;
            }
            if (sysop == 3) {
                gen_next_pc(ctx); gen_helper_halt(tcg_env);
                ctx->base.is_jmp = DISAS_NORETURN; return;
            }
            if (sysop == 4) {
                gen_helper_breakpoint(tcg_env, tcg_constant_i32(pc));
                ctx->base.is_jmp = DISAS_NORETURN; return;
            }
            gen_invalid(ctx, insn); return;
        }
    case 10: /* SYSCALL */
        /* The exception entry path reads the complete interrupted register
         * image from CPUArchState.  Materialize the translated R15 before
         * entering the non-returning helper; unlike ordinary helper calls,
         * syscall does not return to this TB for TCG to flush the global. */
        tcg_gen_st_i32(cpu_r[15], tcg_env,
                       offsetof(CPUMyEmulator32State, r[15]));
        gen_helper_syscall(tcg_env, tcg_constant_i32(extract32(insn, 0, 26)),
                           tcg_constant_i32(pc));
        ctx->base.is_jmp = DISAS_NORETURN;
        return;
    case 11: /* TLBFLUSH */
        {
            unsigned page = extract32(insn, 25, 1);
            unsigned flush_ra = extract32(insn, 20, 5);

            if (extract32(insn, 0, 20) != 0 ||
                (page == 0 && flush_ra != 0)) {
                gen_invalid(ctx, insn); return;
            }
            gen_helper_tlbflush(tcg_env, tcg_constant_i32(page),
                                tcg_constant_i32(flush_ra),
                                tcg_constant_i32(pc));
            gen_next_pc(ctx);
            ctx->base.is_jmp = DISAS_EXIT;
            return;
        }
    case 12: /* CAS */
        if (rd > 15 || ra > 15 || rb > 15 || extract32(insn, 0, 11) != 0) {
            gen_invalid(ctx, insn); return;
        }
        address = gen_reg(rb);
        /* CAS is a faulting memory operation just like loads and stores.
         * Materialize the visible stack pointer before a first-touch page
         * fault so exception entry saves the current user stack state. */
        gen_materialize_fault_state();
        gen_checked_data(ctx, address, 4);
        tmp = tcg_temp_new_i32();
        tcg_gen_atomic_cmpxchg_i32(tmp, address, gen_reg(rd), gen_reg(ra),
                                    myemu32_mmu_idx(ctx), MO_LEUL);
        gen_write_reg(rd, tmp);
        gen_next_pc(ctx);
        return;
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
    tcg_gen_ld_i32(cpu_r[15], tcg_env,
                   offsetof(CPUMyEmulator32State, r[15]));
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
