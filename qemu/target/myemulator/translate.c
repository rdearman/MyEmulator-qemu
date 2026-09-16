#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu/bitops.h"
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
static TCGv_i32 cpu_zf;
static TCGv_i32 cpu_of;
static TCGv_i32 cpu_cf;

void myemulator_cpu_tcg_init(void)
{
    static const char * const names[4] = { "r0", "r1", "r2", "r3" };
    int i;

    cpu_pc = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, pc), "pc");
    cpu_sp = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, sp), "sp");
    cpu_lr = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, lr), "lr");
    cpu_zf = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, zf), "zf");
    cpu_of = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, of), "of");
    cpu_cf = tcg_global_mem_new_i32(tcg_env,
                                    offsetof(CPUMyEmulatorState, cf), "cf");
    for (i = 0; i < 4; i++) {
        cpu_r[i] = tcg_global_mem_new_i32(tcg_env,
                                          offsetof(CPUMyEmulatorState, r[i]),
                                          names[i]);
    }
}

static void gen_next_pc(DisasContext *ctx)
{
    tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next & 0xffff);
}

static void decode_and_translate(DisasContext *ctx)
{
    uint16_t insn = translator_lduw(ctx->env, &ctx->base, ctx->base.pc_next);
    uint8_t opcode = extract32(insn, 12, 4);
    uint8_t regs = extract32(insn, 8, 4);
    uint8_t rd = extract32(regs, 2, 2);
    uint8_t rn = extract32(regs, 0, 2);
    uint8_t imm = extract32(insn, 0, 8);
    TCGv_i32 tmp;

    tcg_gen_insn_start(ctx->base.pc_next, 0, 0);
    ctx->base.pc_next = (ctx->base.pc_next + 2) & 0xffff;

    switch (opcode) {
    case 0x1: /* LI */
        tcg_gen_movi_i32(cpu_r[rd], imm);
        gen_next_pc(ctx);
        break;
    case 0x3: /* ADD immediate, matching the Python emulator path */
        tmp = tcg_temp_new_i32();
        tcg_gen_addi_i32(tmp, cpu_r[rn], imm);
        tcg_gen_setcondi_i32(TCG_COND_GTU, cpu_cf, tmp, 255);
        tcg_gen_setcondi_i32(TCG_COND_GTU, cpu_of, tmp, 127);
        tcg_gen_andi_i32(cpu_r[rd], tmp, 0xff);
        tcg_gen_setcondi_i32(TCG_COND_EQ, cpu_zf, cpu_r[rd], 0);
        gen_next_pc(ctx);
        break;
    case 0x2: /* ST */
        tmp = tcg_temp_new_i32();
        tcg_gen_andi_i32(tmp, cpu_r[rd], 0xff);
        tcg_gen_qemu_st_i32(tmp, cpu_r[rn], MMU_PHYS_IDX, MO_UB);
        gen_next_pc(ctx);
        break;
    case 0x5: /* JMP, with 0xff as the milestone halt convention */
        tcg_gen_movi_i32(cpu_lr, ctx->base.pc_next - 2);
        tcg_gen_movi_i32(cpu_pc, imm);
        if (imm == 0xff) {
            gen_helper_halt(tcg_env);
            ctx->base.is_jmp = DISAS_NORETURN;
        } else {
            ctx->base.is_jmp = DISAS_EXIT;
        }
        break;
    case 0x0: /* Treat all-zero reset memory as a harmless NOP for monitor use. */
        gen_next_pc(ctx);
        break;
    default:
        tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next - 2);
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

void myemulator_translate_code(CPUState *cs, TranslationBlock *tb,
                               int *max_insns, vaddr pc, void *host_pc)
{
    DisasContext dc = { };

    translator_loop(cs, tb, max_insns, pc, host_pc, &myemulator_tr_ops,
                    &dc.base, TCG_TYPE_VA);
}
