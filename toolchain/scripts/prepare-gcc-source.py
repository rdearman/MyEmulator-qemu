#!/usr/bin/env python3
"""Prepare an external GCC release for the maintained MyEmulator2 port."""

from pathlib import Path
import re
import shutil
import sys


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: prepare-gcc-source.py GCC_SOURCE")
    root = Path(sys.argv[1]).resolve()
    fragment = Path(__file__).resolve().parents[1] / "gcc"
    target = root / "gcc/config/myemulator2"
    target.mkdir(parents=True, exist_ok=True)

    for name in ("moxie.cc", "moxie-protos.h", "moxie.opt", "moxie.opt.urls"):
        source = root / "gcc/config/moxie" / name
        destination = target / name.replace("moxie", "myemulator2")
        text = source.read_text().replace("MOXIE", "MYEMULATOR2").replace("Moxie", "MyEmulator2").replace("moxie", "myemulator2")
        destination.write_text(text)

    shutil.copyfile(fragment / "myemulator2.h", target / "myemulator2.h")
    shutil.copyfile(fragment / "myemulator2.md", target / "myemulator2.md")
    shutil.copyfile(fragment / "myemulator2-protos.h", target / "myemulator2-protos.h")
    (target / "t-myemulator2").write_text("# MyEmulator2 has no multilib variants yet.\n")
    for name in ("constraints.md", "predicates.md"):
        source = fragment / name
        (target / name).write_text(source.read_text() if source.exists() else "\n")

    cc = target / "myemulator2.cc"
    text = cc.read_text()
    text = text.replace('#include "expr.h"',
                        '#include "expr.h"\n#include "optabs.h"', 1)
    # MyEmulator2 load/store instructions carry a signed 13-bit byte
    # displacement.  The reference Moxie backend accepts a wider offset;
    # retaining that predicate emits encodings whose high bit is interpreted
    # as a negative displacement by the MyEmulator2 CPU.
    text = text.replace(
        "Return true for memory offset addresses between -32768 and 32767.",
        "Return true for memory offset addresses between -4096 and 4095.")
    text = text.replace(
        "unsigned int v = INTVAL (x) & 0xFFFF8000;\n\t  return (v == 0xFFFF8000 || v == 0x00000000);",
        "return IN_RANGE (INTVAL (x), -4096, 4095);")
    text = text.replace(
        "&& IN_RANGE (INTVAL (XEXP (x, 1)), -32768, 32767))",
        "&& IN_RANGE (INTVAL (XEXP (x, 1)), -4096, 4095))")
    text = text.replace(
        "return regno >= MYEMU2_R1 && regno <= MYEMU2_R11;",
        "return regno >= MYEMU2_R1 && regno <= MYEMU2_R15;")
    text = text.replace("gen_rtx_REG (TYPE_MODE (valtype), MYEMULATOR2_R0)", "gen_rtx_REG (TYPE_MODE (valtype), MYEMU2_R1)")
    text = text.replace("gen_rtx_REG (mode, MYEMULATOR2_R0)", "gen_rtx_REG (mode, MYEMU2_R1)")
    text = text.replace("regno == MYEMULATOR2_R0", "regno == MYEMU2_R1")
    text = text.replace("MYEMULATOR2_R12", "MYEMU2_R12").replace("MYEMULATOR2_R13", "MYEMU2_FP")
    text = text.replace("MYEMULATOR2_SP", "MYEMU2_SP").replace("MYEMULATOR2_R5", "MYEMU2_R5")
    text = text.replace("MYEMULATOR2_R6", "MYEMU2_R5").replace("MYEMULATOR2_R0", "MYEMU2_R0")
    text = text.replace("MYEMULATOR2_FUNCTION_ARG_SIZE", "MYEMU2_FUNCTION_ARG_SIZE")
    text = text.replace("moxie_", "myemulator2_").replace("Moxie", "MyEmulator2")
    text = re.sub(r"static rtx\nmyemulator2_function_arg \(.*?\n}\n\n#define MYEMU2_FUNCTION_ARG_SIZE",
                  """static rtx
myemulator2_function_arg (cumulative_args_t cum_v, const function_arg_info &arg)
{
  CUMULATIVE_ARGS *cum = get_cumulative_args (cum_v);
  if (*cum >= MYEMU2_R1 && *cum <= MYEMU2_R4)
    return gen_rtx_REG (arg.mode, *cum);
  return NULL_RTX;
}

#define MYEMU2_FUNCTION_ARG_SIZE""", text, flags=re.S)
    text = re.sub(r"static void\nmyemulator2_function_arg_advance \(.*?\n}\n\n/\* Return non-zero",
                  """static void
myemulator2_function_arg_advance (cumulative_args_t cum_v,
                                  const function_arg_info &arg)
{
  CUMULATIVE_ARGS *cum = get_cumulative_args (cum_v);
  unsigned bytes = MYEMU2_FUNCTION_ARG_SIZE (arg.mode, arg.type);
  unsigned words = (bytes + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
  if (*cum < MYEMU2_R5)
    *cum += words;
}

/* Return non-zero""", text, flags=re.S)
    text = text.replace("(CUM = MYEMU2_R0)", "(CUM = MYEMU2_R1)")
    text = text.replace("(R == MYEMU2_R5)", "(R == MYEMU2_LR)")
    text = text.replace(
        "  *p1 = CC_REG;\n  *p2 = INVALID_REGNUM;\n  return true;",
        "  *p1 = INVALID_REGNUM;\n  *p2 = INVALID_REGNUM;\n  return false;")
    # MyEmulator2 uses callee-saved r12 as the compiler frame pointer.  The
    # incoming r12 and LR are saved in reserved low frame words.
    text = text.replace(
        "#define TARGET_FRAME_POINTER_REQUIRED hook_bool_void_false",
        "#define TARGET_FRAME_POINTER_REQUIRED hook_bool_void_true")
    marker = "  }\n}\n\nvoid\nmyemulator2_expand_epilogue"
    replacement = "  }\n\n  /* Stack arguments occupy the bottom of the frame.  Keep the fixed\n     LR/frame header above them. */\n  HOST_WIDE_INT header = crtl->outgoing_args_size;\n  emit_move_insn (gen_rtx_MEM (SImode,\n                               plus_constant (Pmode, stack_pointer_rtx, header)),\n                  gen_rtx_REG (SImode, MYEMU2_LR));\n  emit_move_insn (gen_rtx_MEM (SImode,\n                               plus_constant (Pmode, stack_pointer_rtx, header + 4)),\n                  gen_rtx_REG (SImode, MYEMU2_R15));\n}\n\nvoid\nmyemulator2_expand_epilogue"
    if marker not in text:
        raise SystemExit("could not locate prologue end")
    text = text.replace(marker, replacement, 1)
    text = text.replace(
        "  cfun->machine->size_for_adjusting_sp =\n    crtl->args.pretend_args_size\n",
        "  cfun->machine->size_for_adjusting_sp =\n    crtl->args.pretend_args_size\n", 1)
    text = text.replace(
        "       ? (HOST_WIDE_INT) crtl->outgoing_args_size : 0);\n}",
        "       ? (HOST_WIDE_INT) crtl->outgoing_args_size : 0);\n\n  /* Reserve a fixed entry header above the register-argument home area. */\n  cfun->machine->size_for_adjusting_sp += 48;\n}", 1)
    text = text.replace(
        "  myemulator2_compute_frame ();\n\n  if (flag_stack_usage_info)",
        "  myemulator2_compute_frame ();\n\n  /* Preserve the incoming frame register in a caller-saved temporary,\n     then use the incoming SP as the frame base. */\n  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_R15),\n                  gen_rtx_REG (SImode, MYEMU2_FP));\n  emit_move_insn (hard_frame_pointer_rtx, stack_pointer_rtx);\n\n  if (flag_stack_usage_info)", 1)
    text = text.replace(
        "  emit_move_insn (hard_frame_pointer_rtx, stack_pointer_rtx);",
        "  emit_move_insn (hard_frame_pointer_rtx, stack_pointer_rtx);", 1)
    text = text.replace(
        "  int regno;\n  rtx reg;\n\n  if (cfun->machine->callee_saved_reg_size",
        "  int regno;\n  rtx reg;\n\n  /* Restore the incoming LR before releasing the frame. */\n  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_LR),\n                  gen_rtx_MEM (SImode, stack_pointer_rtx));\n\n  if (cfun->machine->callee_saved_reg_size", 1)
    text = text.replace(
        "  emit_jump_insn (gen_returner ());\n}",
        "  /* Restore the caller's frame pointer, then release this function's\n     complete frame.  The saved frame pointer is not the caller's SP: the\n     latter is the incoming SP plus both the local/outgoing area and any\n     callee-save pushes. */\n  emit_move_insn (hard_frame_pointer_rtx,\n                  gen_rtx_MEM (SImode, plus_constant (Pmode, stack_pointer_rtx, 4)));\n  {\n    HOST_WIDE_INT frame_release =\n      cfun->machine->size_for_adjusting_sp\n      + cfun->machine->callee_saved_reg_size;\n    if (frame_release <= 2047)\n      emit_insn (gen_addsi3 (stack_pointer_rtx, stack_pointer_rtx,\n                             GEN_INT (frame_release)));\n    else\n      {\n        rtx scratch = gen_rtx_REG (SImode, MYEMU2_R12);\n        emit_move_insn (scratch, GEN_INT (frame_release));\n        emit_insn (gen_addsi3 (stack_pointer_rtx, stack_pointer_rtx, scratch));\n      }\n  }\n  emit_jump_insn (gen_returner ());\n}", 1)
    text = text.replace(
        "  else if ((from) == ARG_POINTER_REGNUM && (to) == HARD_FRAME_POINTER_REGNUM)\n    ret = 0x00;\n  else\n    abort ();",
        "  else if ((from) == ARG_POINTER_REGNUM && (to) == HARD_FRAME_POINTER_REGNUM)\n    ret = 16;\n  else if ((from) == FRAME_POINTER_REGNUM && (to) == STACK_POINTER_REGNUM)\n    { myemulator2_compute_frame (); ret = 0; }\n  else if ((from) == ARG_POINTER_REGNUM && (to) == STACK_POINTER_REGNUM)\n    ret = 16;\n  else\n    ret = 0;")
    # The MyEmulator2 load/store syntax requires a base register.  Do not
    # describe bare symbolic addresses as legitimate memory operands; reload
    # will materialise them with the target's LI sequence first.
    text = text.replace(
        "  if (GET_CODE (x) == SYMBOL_REF\n      || GET_CODE (x) == LABEL_REF\n      || GET_CODE (x) == CONST)\n    return true;\n",
        "")
    text = text.replace(
        "if (REGNO (operand) > MYEMU2_FP)",
        "if (REGNO (operand) >= FIRST_PSEUDO_REGISTER)")
    # GCC's inherited frame/argument-pointer machinery can leave a named
    # pseudo register in final RTL.  It is an implementation detail, not an
    # architectural register; print it through the physical frame register
    # rather than leaking names such as "argp" into GNU as input.
    text = text.replace(
        "static void\nmyemulator2_print_operand_address",
        "static const char *\nmyemulator2_reg_name (unsigned int regno)\n{\n  if (regno >= FIRST_PSEUDO_REGISTER)\n    return \"r15\";\n  return reg_names[regno];\n}\n\nstatic void\nmyemulator2_print_operand_address")
    text = text.replace("reg_names[REGNO (x)]", "myemulator2_reg_name (REGNO (x))")
    text = text.replace("reg_names[REGNO (XEXP (x, 0))]",
                        "myemulator2_reg_name (REGNO (XEXP (x, 0)))")
    text = text.replace("reg_names[REGNO (operand)]",
                        "myemulator2_reg_name (REGNO (operand))")
    text = text.replace(
        '  if (regno >= FIRST_PSEUDO_REGISTER)\n    return "r15";\n  return reg_names[regno];',
        '  if (regno == MYEMU2_AP)\n    return "sp";\n  if (regno == MYEMU2_FRAME || regno >= FIRST_PSEUDO_REGISTER)\n    return "r15";\n  return reg_names[regno];')
    text = text.replace("if (regno >= FIRST_PSEUDO_REGISTER)",
                        "if (regno >= 16)")
    text = text.replace(
        "  /* Frame-relative locals use the caller's incoming SP as their base. */\n  emit_move_insn (hard_frame_pointer_rtx, stack_pointer_rtx);",
        "  /* Preserve the incoming frame register in a caller-saved temporary.\n     Keep a 16-byte register-argument home area above the incoming SP so\n     recursive callees cannot overwrite the caller's saved link register. */\n  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_R15),\n                  gen_rtx_REG (SImode, MYEMU2_FP));\n  emit_move_insn (hard_frame_pointer_rtx, stack_pointer_rtx);\n  emit_insn (gen_addsi3 (hard_frame_pointer_rtx, hard_frame_pointer_rtx,\n                         GEN_INT (16)));", 1)
    text = text.replace(
        "  /* Restore the incoming LR before releasing the frame. */",
        "  /* Restore LR and the incoming frame register. */", 1)
    text = text.replace(
        "  emit_move_insn (stack_pointer_rtx, hard_frame_pointer_rtx);\n  emit_move_insn (hard_frame_pointer_rtx, gen_rtx_REG (SImode, MYEMU2_R15));\n  emit_jump_insn (gen_returner ());",
        "  /* Restore the caller's frame pointer, then release this function's\n     complete frame.  The saved frame pointer is not the caller's SP: the\n     latter is the incoming SP plus both the local/outgoing area and any\n     callee-save pushes. */\n  emit_move_insn (hard_frame_pointer_rtx,\n                  gen_rtx_MEM (SImode, plus_constant (Pmode, stack_pointer_rtx, 4)));\n  {\n    HOST_WIDE_INT frame_release =\n      cfun->machine->size_for_adjusting_sp\n      + cfun->machine->callee_saved_reg_size;\n    if (frame_release <= 2047)\n      emit_insn (gen_addsi3 (stack_pointer_rtx, stack_pointer_rtx,\n                             GEN_INT (frame_release)));\n    else\n      {\n        rtx scratch = gen_rtx_REG (SImode, MYEMU2_R12);\n        emit_move_insn (scratch, GEN_INT (frame_release));\n        emit_insn (gen_addsi3 (stack_pointer_rtx, stack_pointer_rtx, scratch));\n      }\n  }\n  emit_jump_insn (gen_returner ());", 1)
    # The saved LR/frame header sits above the outgoing argument area.  Apply
    # this final rewrite after the compatibility substitutions above so the
    # generated backend cannot regress to a header at offset zero.
    text = text.replace(
        """  /* The low words hold the incoming LR and frame-register value. */
  emit_move_insn (gen_rtx_MEM (SImode, stack_pointer_rtx),
                  gen_rtx_REG (SImode, MYEMU2_LR));
  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (Pmode, stack_pointer_rtx, 4)),
                  gen_rtx_REG (SImode, MYEMU2_R15));""",
        """  /* Stack arguments occupy the bottom of the frame. */
  HOST_WIDE_INT header = 48;
  emit_move_insn (gen_rtx_MEM (SImode,
                               plus_constant (Pmode, stack_pointer_rtx, header)),
                  gen_rtx_REG (SImode, MYEMU2_LR));
  emit_move_insn (gen_rtx_MEM (SImode,
                               plus_constant (Pmode, stack_pointer_rtx, header + 4)),
                  gen_rtx_REG (SImode, MYEMU2_R15));""", 1)
    text = text.replace(
        """  /* Restore the incoming LR before releasing the frame. */
  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_LR),
                  gen_rtx_MEM (SImode, stack_pointer_rtx));""",
        """  /* Restore LR from above the outgoing argument area. */
  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_LR),
                  gen_rtx_MEM (SImode,
                               plus_constant (Pmode, stack_pointer_rtx,
                                              48)));""", 1)
    text = text.replace(
        """  /* Restore LR and the incoming frame register. */
  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_LR),
                  gen_rtx_MEM (SImode, stack_pointer_rtx));""",
        """  /* Restore LR from above the outgoing argument area. */
  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_LR),
                  gen_rtx_MEM (SImode,
                               plus_constant (Pmode, stack_pointer_rtx,
                                              48)));""", 1)
    text = text.replace(
        "gen_rtx_MEM (SImode, plus_constant (Pmode, stack_pointer_rtx, 4))",
        "gen_rtx_MEM (SImode, plus_constant (Pmode, stack_pointer_rtx,\n                                             crtl->outgoing_args_size + 4))", 1)

    text = text.replace(
        "hard_frame_pointer_rtx, hard_frame_pointer_rtx,\n                         GEN_INT (16)",
        "hard_frame_pointer_rtx, hard_frame_pointer_rtx,\n                         GEN_INT (0)", 1)

    # Save the link/frame words at the bottom of the newly allocated frame.
    # The caller's outgoing argument area begins at the incoming SP, while
    # locals are addressed from that incoming SP, so offsets 0 and 4 from
    # the final SP are the only stable non-overlapping header locations.
    text = text.replace(
        "  HOST_WIDE_INT header = crtl->outgoing_args_size;",
        "  HOST_WIDE_INT header = 0;", 1)
    text = text.replace(
        "52))",
        "4)))", 1)
    text = text.replace(
        "                                              48)));",
        "                                              0)));", 1)
    text = text.replace(
        "                                             crtl->outgoing_args_size + 4)));",
        "                                             4)));", 1)

    target_hooks = r'''
/* DImode values occupy adjacent 32-bit general registers.  The fixed
   system registers are not valid members of a register pair. */
static unsigned int
myemulator2_hard_regno_nregs (unsigned int regno, machine_mode mode)
{
  (void) regno;
  return (GET_MODE_SIZE (mode) + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
}

static bool
myemulator2_hard_regno_mode_ok (unsigned int regno, machine_mode mode)
{
  unsigned int nregs = myemulator2_hard_regno_nregs (regno, mode);
  if (nregs == 1)
    return regno >= MYEMU2_R1 && regno <= MYEMU2_R15;
  return regno >= MYEMU2_R1 && regno + nregs <= 12;
}

#undef TARGET_HARD_REGNO_NREGS
#define TARGET_HARD_REGNO_NREGS myemulator2_hard_regno_nregs
#undef TARGET_HARD_REGNO_MODE_OK
#define TARGET_HARD_REGNO_MODE_OK myemulator2_hard_regno_mode_ok
'''
    text = text.replace("struct gcc_target targetm = TARGET_INITIALIZER;", target_hooks + "\nstruct gcc_target targetm = TARGET_INITIALIZER;")
    text = text.replace("MYEMU2_R15", "MYEMU2_R12")
    text = text.replace(
        "  int regs = 8 - *cum;",
        "  /* Only R1-R4 are argument registers.  Do not save R5-R7 as if\n"
        "     they were incoming arguments in variadic calls. */\n"
        "  int regs = MYEMU2_R5 - *cum;")
    text = text.replace("for (regno = *cum; regno < 8; regno++)",
                        "for (regno = *cum; regno < MYEMU2_R5; regno++)")
    text = text.replace("if (*cum >= 8)", "if (*cum >= MYEMU2_R5)")
    text = text.replace(
        "GEN_INT (UNITS_PER_WORD * (3 + (regno-2)))",
        "GEN_INT (UNITS_PER_WORD * (1 + (regno-2)))")
    text = text.replace("bytes_left = (4 * 6) - ((*cum - 2) * 4);",
                        "bytes_left = (MYEMU2_R5 - MYEMU2_R1) * 4\n"
                        "               - ((*cum - MYEMU2_R1) * 4);")
    va_hook = r'''
/* The generic va_start calculation already points at the first saved
   variadic register slot after the fixed argument area. */
static void
myemulator2_va_start (tree valist, rtx nextarg)
{
  nextarg = plus_constant (Pmode, nextarg, 0);
  std_expand_builtin_va_start (valist, nextarg);
}

'''
    text = text.replace("/* Worker function for TARGET_STATIC_CHAIN.  */", va_hook + "/* Worker function for TARGET_STATIC_CHAIN.  */", 1)
    text = text.replace(
        "#undef  TARGET_SETUP_INCOMING_VARARGS\n#define TARGET_SETUP_INCOMING_VARARGS \tmyemulator2_setup_incoming_varargs",
        "#undef  TARGET_SETUP_INCOMING_VARARGS\n#define TARGET_SETUP_INCOMING_VARARGS \tmyemulator2_setup_incoming_varargs\n#undef  TARGET_EXPAND_BUILTIN_VA_START\n#define TARGET_EXPAND_BUILTIN_VA_START myemulator2_va_start")
    # The frame-expansion compatibility rewrite above deliberately uses R12
    # for GCC's internal scratch references.  Keep the architectural fixed
    # registers (SP/LR) valid for local-register declarations and Linux's
    # current_thread_info implementation.
    text = text.replace(
        "return regno >= MYEMU2_R1 && regno <= MYEMU2_R12;\n  return regno >= MYEMU2_R1 && regno + nregs <= 12;",
        "return regno >= MYEMU2_R1 && regno <= MYEMU2_R15;\n  return regno >= MYEMU2_R1 && regno + nregs <= 12;")
    marker = "/* The Global `targetm' Variable.  */"
    helper = r'''
void
myemulator2_expand_cbranchdf4 (rtx *operands)
{
  enum rtx_code code = GET_CODE (operands[0]);
  const char *name;
  switch (code)
    {
    case EQ: name = "__eqdf2"; break;
    case NE: name = "__nedf2"; break;
    case LT: name = "__ltdf2"; break;
    case LE: name = "__ledf2"; break;
    case GT: name = "__gtdf2"; break;
    case GE: name = "__gedf2"; break;
    default: gcc_unreachable ();
    }
  rtx libfunc = gen_rtx_SYMBOL_REF (Pmode, name);
  rtx cmp = emit_library_call_value (libfunc, NULL_RTX, LCT_CONST,
                                     SImode, operands[1], DFmode,
                                     operands[2], DFmode);
  emit_cmp_and_jump_insns (cmp, const0_rtx, code, NULL_RTX, SImode, 0,
                           operands[3]);
}

'''
    if marker not in text:
        raise SystemExit("could not locate target hook marker")
    text = text.replace(marker, helper + marker, 1)
    cc.write_text(text)

    config_gcc = root / "gcc/config.gcc"
    text = config_gcc.read_text()
    text = text.replace("moxie*)\tcpu_type=moxie", "myemulator2*)\tcpu_type=myemulator2\n\ttarget_has_targetm_common=no\n\t;;\nmoxie*)\tcpu_type=moxie")
    if "myemulator2-*-elf)" not in text:
        text = text.replace("moxie-*-elf)\n", "myemulator2-*-elf)\n\tgas=yes\n\tgnu_ld=yes\n\ttm_file=\"elfos.h newlib-stdint.h ${tm_file}\"\n\ttmake_file=\"${tmake_file} myemulator2/t-myemulator2\"\n\t;;\nmoxie-*-elf)\n", 1)
    config_gcc.write_text(text)

    config_sub = root / "config.sub"
    text = config_sub.read_text()
    if "myemulator2" not in text:
        marker = "\t\t\t| moxie " + "\\" + "\n"
        replacement = "\t\t\t| myemulator2 " + "\\" + "\n" + marker
        if marker not in text:
            raise SystemExit("could not find config.sub machine-name list")
        text = text.replace(marker, replacement, 1)
    config_sub.write_text(text)

    host = root / "libgcc/config.host"
    text = host.read_text()
    if "myemulator2*) cpu_type=myemulator2" not in text:
        marker = "moxie*)"
        if marker not in text:
            raise SystemExit("could not find libgcc moxie cpu_type case")
        text = text.replace(marker, "myemulator2*) cpu_type=myemulator2\n\t;;\n" + marker, 1)
    if "myemulator2-*-elf" not in text:
        text = text.replace("moxie-*-elf | moxie-*-moxiebox* | moxie-*-uclinux* | moxie-*-rtems*)",
                            "myemulator2-*-elf | moxie-*-elf | moxie-*-moxiebox* | moxie-*-uclinux* | moxie-*-rtems*)")
        text = re.sub(r"moxie/t-moxie", "myemulator2/t-myemulator2", text, count=1)
    # The copied Moxie configuration also enables soft-fp support.  MyEmulator2
    # has no floating-point ISA, so the target libgcc must remain integer-only
    # rather than attempting to build software-float entry points.
    text = text.replace(
        "myemulator2/t-myemulator2 t-softfp-sfdf t-softfp-excl t-softfp",
        "myemulator2/t-myemulator2")
    host.write_text(text)

    libgcc_target = root / "libgcc/config/myemulator2"
    libgcc_target.mkdir(parents=True, exist_ok=True)
    libgcc_source = root / "libgcc/config/moxie"
    if libgcc_source.exists():
        for source in libgcc_source.iterdir():
            if source.is_file():
                destination = libgcc_target / source.name.replace("moxie", "myemulator2")
                transformed = source.read_text().replace("MOXIE", "MYEMULATOR2").replace("Moxie", "MyEmulator2").replace("moxie", "myemulator2")
                destination.write_text(transformed)
    (libgcc_target / "t-myemulator2").write_text(
        """# MyEmulator2 v1 is an integer/freestanding target.  The CPU and
# ABI do not define floating-point operations yet, so do not build libgcc's
# complex-float helper entry points.  Integer and 64-bit integer helpers are
# still built normally.
# Exclude the floating-point names present in lib2funcs.  This is written as
# a deferred make expression because lib2funcs is defined later in the
# generated Makefile.
LIB2FUNCS_EXCLUDE += $(foreach f,$(lib2funcs),$(if $(or $(findstring sf,$f),$(findstring df,$f),$(findstring tf,$f)),$f))
LIB2FUNCS_EXCLUDE += _mulhc3 _mulsc3 _muldc3 _mulxc3 _multc3
LIB2FUNCS_EXCLUDE += _divhc3 _divsc3 _divdc3 _divxc3 _divtc3
# The scalar conversion families are generated from swfloatfuncs/dwfloatfuncs
# rather than appearing in lib2funcs; list their integer-mode forms too.
LIB2FUNCS_EXCLUDE += _fixsfsi _fixunssfsi _fixdfsi _fixunsdfsi
LIB2FUNCS_EXCLUDE += _fixsfdi _fixunssfdi _fixdfdi _fixunsdfdi
LIB2FUNCS_EXCLUDE += _floatsisf _floatunsisf _floatsidf _floatunsidf
 # Freestanding MyEmulator2 has no exception/unwind ABI yet.  GCC C tests
 # use -fno-exceptions; omit inherited EH objects until that ABI exists.
LIB2ADDEH =
LIB2ADDEHSTATIC =
LIB2ADDEHSHARED =
""")

    # GCC 15.2.0's generated libcpp configure script probes a dependency
    # helper that is absent from the release's nested build tree.  The
    # compiler build does not need generated dependency files, so make this
    # host-only probe deterministic instead of depending on that helper.
    # A few GCC 15 sub-configures contain the same host-only dependency
    # probe.  Disable it consistently for the cross compiler build; this has
    # no effect on target code generation or the installed compiler.
    for configure_path in (root / "libcpp/configure",
                           root / "gcc/configure"):
        if not configure_path.exists():
            continue
        text = configure_path.read_text()
        needle = "if ${am_cv_CXX_dependencies_compiler_type+:} false; then :"
        if needle in text:
            text = text.replace(needle, "am_cv_CXX_dependencies_compiler_type=none\nif true; then :", 1)
        text = text.replace(
            'then as_fn_error $? "no usable dependency style found" "$LINENO" 5\n'
            'else CXXDEPMODE=depmode=$am_cv_CXX_dependencies_compiler_type',
            'then CXXDEPMODE=depmode=none\n'
            'else CXXDEPMODE=depmode=$am_cv_CXX_dependencies_compiler_type',
            1)
        configure_path.write_text(text)

    # libcpp's cached host probe can incorrectly conclude that ptrdiff_t is
    # absent.  The host C++ headers provide it; forcing this probe prevents
    # the generated config.h from defining ptrdiff_t as a 32-bit int, which
    # would corrupt host pointer arithmetic while building libcpp.
    libcpp_configure = root / "libcpp/configure"
    if libcpp_configure.exists():
        text = libcpp_configure.read_text()
        text = text.replace(
            '  ac_fn_c_check_type "$LINENO" "uintptr_t"',
            '  ac_cv_type_uintptr_t=yes\n  ac_fn_c_check_type "$LINENO" "uintptr_t"', 1)
        text = text.replace(
            'ac_fn_c_find_uintX_t "$LINENO" "64"',
            'ac_cv_c_uint64_t=yes\nac_fn_c_find_uintX_t "$LINENO" "64"', 1)
        marker = 'ac_fn_c_check_type "$LINENO" "ptrdiff_t"'
        if marker in text and 'ac_cv_type_ptrdiff_t=yes\n' not in text:
            text = text.replace(marker,
                                'ac_cv_type_ptrdiff_t=yes\n' + marker, 1)
        libcpp_configure.write_text(text)

    # GCC 15.2.0's host C++ compilation of libcpp/charset.cc uses free()
    # without including its declaring header on this host toolchain.  Keep
    # the source preparation reproducible; this is host-build hygiene and
    # does not affect MyEmulator2 code generation.
    charset = root / "libcpp/charset.cc"
    if charset.exists():
        text = charset.read_text()
        text = text.replace("#include <cstdlib>\n", "")
        charset.write_text(text)

    # The generated host config can leave HAVE_STDLIB_H unset even though
    # the host provides it.  system.h consequently omits the declaration of
    # free(), which GCC 15's C++ libcpp sources use directly.
    system_h = root / "libcpp/system.h"
    if system_h.exists():
        text = system_h.read_text()
        if "#include <stdlib.h> /* MyEmulator2 host build */" not in text:
            text = text.replace(
                "#include <stdarg.h>\n",
                "#include <stdarg.h>\n#include <stdlib.h> /* MyEmulator2 host build */\n", 1)
            system_h.write_text(text)


if __name__ == "__main__":
    main()
