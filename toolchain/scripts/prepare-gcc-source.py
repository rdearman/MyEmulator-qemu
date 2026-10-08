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
    shutil.copyfile(fragment / "myemulator2-linux.h", target / "myemulator2-linux.h")
    shutil.copyfile(fragment / "myemulator2.md", target / "myemulator2.md")
    shutil.copyfile(fragment / "myemulator2-protos.h", target / "myemulator2-protos.h")
    libgcc_target = root / "libgcc/config/myemulator2"
    libgcc_target.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(fragment / "sfp-machine.h", libgcc_target / "sfp-machine.h")
    (target / "t-myemulator2").write_text("# MyEmulator2 has no multilib variants yet.\n")
    libgcc_host = root / "libgcc/config.host"
    host_text = libgcc_host.read_text()
    old = 'myemulator2-*-elf | moxie-*-elf | moxie-*-moxiebox* | moxie-*-uclinux* | moxie-*-rtems*)\n\ttmake_file="$tmake_file myemulator2/t-myemulator2"'
    new = 'myemulator2-*-elf)\n\ttmake_file="$tmake_file myemulator2/t-myemulator2 t-softfp-sfdf t-softfp"'
    # GCC source trees may be fresh, or may already have been prepared by a
    # previous invocation.  Accept both forms and make the transformation
    # idempotent instead of requiring one exact upstream line layout.
    if old in host_text:
        host_text = host_text.replace(old, new, 1)
    elif not re.search(r"(?m)^myemulator2-\*-elf\)", host_text):
        moxie_case = re.compile(
            r"(?m)^(moxie-\*-elf \| moxie-\*-moxiebox\* \| "
            r"moxie-\*-uclinux\* \| moxie-\*-rtems\*)\)\n")
        host_text, count = moxie_case.subn(
            "myemulator2-*-elf)\n"
            "\ttmake_file=\"$tmake_file myemulator2/t-myemulator2 "
            "t-softfp-sfdf t-softfp\"\n"
            "\t;;\n\\1)\n", host_text, count=1)
        if count != 1:
            raise SystemExit("could not locate MyEmulator2 libgcc target")
    # Repair the form produced by older versions of this preparation script.
    host_text = host_text.replace(
        "moxie-*-elf | moxie-*-moxiebox* | moxie-*-uclinux* | moxie-*-rtems*\n",
        "moxie-*-elf | moxie-*-moxiebox* | moxie-*-uclinux* | moxie-*-rtems*)\n",
        1)
    libgcc_host.write_text(host_text)

    # Register both the bare ELF toolchain and the Linux-musl target.  The
    # bare case is required for the first-stage compiler used to build musl;
    # keeping it in the preparation step prevents isolated rebuilds from
    # depending on a manually modified generated source tree.
    config_gcc = root / "gcc/config.gcc"
    config_text = config_gcc.read_text()
    bare_case = """myemulator2-*-elf)
\tgas=yes
\tgnu_ld=yes
\ttm_file=\"elfos.h newlib-stdint.h ${tm_file}\"
\ttmake_file=\"${tmake_file} myemulator2/t-myemulator2 t-softfp-sfdf t-softfp\"
\t;;
"""
    linux_case = """myemulator2-*-linux-musl*)
\tgas=yes
\tgnu_ld=yes
\ttm_file=\"elfos.h gnu-user.h linux.h glibc-stdint.h ${tm_file} myemulator2/myemulator2-linux.h\"
\ttmake_file=\"${tmake_file} myemulator2/t-myemulator2-linux t-softfp-sfdf t-softfp t-linux\"
\t;;
"""
    if "myemulator2-*-elf)" not in config_text or "myemulator2-*-linux-musl*)" not in config_text:
        anchor = "moxie-*-elf)\n"
        if anchor not in config_text:
            raise SystemExit("could not locate insertion point for MyEmulator2 targets in config.gcc")
        if "myemulator2-*-elf)" not in config_text:
            config_text = config_text.replace(anchor, bare_case + anchor, 1)
        if "myemulator2-*-linux-musl*)" not in config_text:
            config_text = config_text.replace(anchor, linux_case + anchor, 1)
        config_gcc.write_text(config_text)
    for name in ("constraints.md", "predicates.md"):
        source = fragment / name
        (target / name).write_text(source.read_text() if source.exists() else "\n")

    cc = target / "myemulator2.cc"
    text = cc.read_text()
    text = text.replace('#include "builtins.h"',
                        '#include "builtins.h"\n#include "config/linux-protos.h"', 1)
    text = text.replace('#include "expr.h"',
                        '#include "expr.h"\n#include "optabs.h"', 1)
    # The REM host ABI has no Linux personality(2) syscall.  GCC's driver
    # probes this host-only reproducibility hook from auto-host.h, but the
    # declaration can still be visible when the Canadian host is REM Linux.
    # Suppress it before the optional include and call are compiled.
    gcc_driver = root / "gcc/gcc.cc"
    driver_text = gcc_driver.read_text()
    driver_old = '#include "system.h"\n#ifdef HOST_HAS_PERSONALITY_ADDR_NO_RANDOMIZE'
    driver_new = ('#include "system.h"\n#include <fcntl.h>\n'
                  '#undef HOST_HAS_PERSONALITY_ADDR_NO_RANDOMIZE\n'
                  '#ifdef HOST_HAS_PERSONALITY_ADDR_NO_RANDOMIZE')
    if driver_old in driver_text:
        driver_text = driver_text.replace(driver_old, driver_new, 1)
    elif "#undef HOST_HAS_PERSONALITY_ADDR_NO_RANDOMIZE" not in driver_text:
        raise SystemExit("could not locate GCC personality probe")
    gcc_driver.write_text(driver_text)
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
        "    + (ACCUMULATE_OUTGOING_ARGS\n       ? (HOST_WIDE_INT) crtl->outgoing_args_size : 0);\n}",
        "    + (HOST_WIDE_INT) crtl->outgoing_args_size;\n\n  /* Reserve a fixed entry header and callee argument-home area. */\n  cfun->machine->size_for_adjusting_sp += 80;\n}", 1)
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
    # Keep the fixed header above the callee argument-home area reserved at
    # the incoming stack pointer.
    text = text.replace(
        "  HOST_WIDE_INT header = crtl->outgoing_args_size;",
        "  HOST_WIDE_INT header = crtl->outgoing_args_size + 32;", 1)
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
  auto emit_call = [&](const char *name) {
    rtx libfunc = gen_rtx_SYMBOL_REF (Pmode, name);
    return emit_library_call_value (libfunc, NULL_RTX, LCT_CONST,
                                    SImode, operands[1], DFmode,
                                    operands[2], DFmode);
  };
  auto emit_branch = [&](rtx cmp, enum rtx_code branch) {
    emit_cmp_and_jump_insns (cmp, const0_rtx, branch, NULL_RTX, SImode, 0,
                             operands[3]);
  };

  switch (code)
    {
    case EQ: emit_branch (emit_call ("__eqdf2"), EQ); return;
    case NE: emit_branch (emit_call ("__nedf2"), NE); return;
    case LT: emit_branch (emit_call ("__ltdf2"), LT); return;
    case LE: emit_branch (emit_call ("__ledf2"), LE); return;
    case GT: emit_branch (emit_call ("__gtdf2"), GT); return;
    case GE: emit_branch (emit_call ("__gedf2"), GE); return;
    case UNORDERED: emit_branch (emit_call ("__unorddf2"), NE); return;
    case ORDERED: emit_branch (emit_call ("__unorddf2"), EQ); return;
    case UNEQ:
      emit_branch (emit_call ("__unorddf2"), NE);
      emit_branch (emit_call ("__eqdf2"), EQ);
      return;
    case UNLT:
      emit_branch (emit_call ("__unorddf2"), NE);
      emit_branch (emit_call ("__ltdf2"), LT);
      return;
    case UNLE:
      emit_branch (emit_call ("__unorddf2"), NE);
      emit_branch (emit_call ("__ledf2"), LE);
      return;
    case UNGT:
      emit_branch (emit_call ("__unorddf2"), NE);
      emit_branch (emit_call ("__gtdf2"), GT);
      return;
    case UNGE:
      emit_branch (emit_call ("__unorddf2"), NE);
      emit_branch (emit_call ("__gedf2"), GE);
      return;
    case LTGT:
      {
        rtx ordered = gen_label_rtx ();
        emit_cmp_and_jump_insns (emit_call ("__unorddf2"), const0_rtx,
                                 EQ, NULL_RTX, SImode, 0, ordered);
        emit_branch (emit_call ("__nedf2"), NE);
        emit_label (ordered);
        return;
      }
    default: gcc_unreachable ();
    }
}

'''
    if marker not in text:
        raise SystemExit("could not locate target hook marker")
    text = text.replace(marker, helper + marker, 1)
    # Preserve the incoming frame pointer in R12 while allocating large
    # frames; reusing R12 for the frame-size temporary makes the epilogue
    # restore that size as R15.  Emit repeated immediate subtracts instead.
    old = ("      else\n"
           "\t{\n"
           "\t  rtx reg = gen_rtx_REG (SImode, MYEMU2_R12);\n"
           "\t  insn = emit_move_insn (reg, GEN_INT (i));\n"
           "\t  RTX_FRAME_RELATED_P (insn) = 1;\n"
           "\t  insn = emit_insn (gen_subsi3 (stack_pointer_rtx,\n"
           "\t\t\t\t\tstack_pointer_rtx,\n"
           "\t\t\t\t\treg));\n"
           "\t  RTX_FRAME_RELATED_P (insn) = 1;\n"
           "\t}")
    new = ("      else\n"
           "\t{\n"
           "\t  while (i > 252)\n"
           "\t    {\n"
           "\t      insn = emit_insn (gen_subsi3 (stack_pointer_rtx,\n"
           "\t\t\t\t\t    stack_pointer_rtx,\n"
           "\t\t\t\t\t    GEN_INT (252)));\n"
           "\t      RTX_FRAME_RELATED_P (insn) = 1;\n"
           "\t      i -= 252;\n"
           "\t    }\n"
           "\t  if (i != 0)\n"
           "\t    {\n"
           "\t      insn = emit_insn (gen_subsi3 (stack_pointer_rtx,\n"
           "\t\t\t\t\t    stack_pointer_rtx,\n"
           "\t\t\t\t\t    GEN_INT (i)));\n"
           "\t      RTX_FRAME_RELATED_P (insn) = 1;\n"
           "\t    }\n"
           "\t}")
    if old not in text:
        raise SystemExit("could not locate large-frame prologue")
    text = text.replace(old, new, 1)
    # The hard frame pointer denotes the incoming stack pointer.  It must be
    # established before either callee-save pushes or local-frame allocation;
    # positive offsets then address the caller's outgoing argument area and
    # remain stable when the function uses alloca()/a VLA.
    fp_line = "  emit_move_insn (hard_frame_pointer_rtx, stack_pointer_rtx);\n"
    if text.count(fp_line) != 1:
        raise SystemExit("unexpected frame-pointer setup count")
    text = text.replace(fp_line, "", 1)
    frame_marker = "  /* Stack arguments occupy the bottom of the frame."
    if frame_marker not in text:
        raise SystemExit("could not locate frame argument marker")
    prologue_marker = ("  myemulator2_compute_frame ();\n"
                       "\n  /* Preserve the incoming frame register")
    if prologue_marker not in text:
        raise SystemExit("could not locate prologue frame setup")
    text = text.replace(
        prologue_marker,
        "  myemulator2_compute_frame ();\n\n"
        + "  /* Preserve the incoming frame register", 1)
    preserve_marker = (
        "  /* Preserve the incoming frame register in a caller-saved temporary,\n"
        "     then use the incoming SP as the frame base. */\n"
        "  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_R12),\n"
        "                  gen_rtx_REG (SImode, MYEMU2_FP));\n")
    if preserve_marker not in text:
        raise SystemExit("could not locate incoming frame preservation")
    text = text.replace(preserve_marker,
                        preserve_marker + "\n" + fp_line, 1)

    # BUG FIX: myemulator2_expand_epilogue() previously restored LR and the
    # caller's frame pointer relative to the *current* stack pointer, and
    # released the frame by adding a compile-time constant to that same
    # current stack pointer. This is only correct when the stack pointer at
    # epilogue entry equals its value immediately after the prologue ran.
    # Any function that calls alloca() or uses a variable-length array (for
    # example busybox's vgetopt32(), used by nearly every applet except
    # "echo") further lowers the stack pointer at runtime; GCC's generic
    # middle-end then resets the stack pointer to its function-entry value
    # (via a register saved for that purpose) immediately before this
    # epilogue runs. The saved LR/FP header is therefore neither at the
    # post-prologue stack pointer position nor at the alloca-adjusted
    # position -- it is only reliably reachable relative to the frame
    # pointer, which is set once in the prologue and never changed
    # afterwards. Restoring SP from FP directly (rather than adding a fixed
    # offset to SP) likewise correctly undoes any dynamic stack allocation.
    # Without this fix, any exec of a non-"echo"-like applet reads garbage
    # (such as argc) as its own return address and jumps to it, producing a
    # misaligned-PC fault (myemulator2 exception cause 2) that this port's
    # traps.c must SIGBUS the process for -- previously misdiagnosed as
    # kernel/QEMU memory corruption ("Bug #5").
    old_epilogue = (
        "void\n"
        "myemulator2_expand_epilogue (void)\n"
        "{\n"
        "  int regno;\n"
        "  rtx reg;\n"
        "\n"
        "  /* Restore LR from above the outgoing argument area. */\n"
        "  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_LR),\n"
        "                  gen_rtx_MEM (SImode,\n"
        "                               plus_constant (Pmode, stack_pointer_rtx,\n"
        "                                              0)));\n"
        "\n"
        "  if (cfun->machine->callee_saved_reg_size != 0)\n"
        "    {\n"
        "      reg = gen_rtx_REG (Pmode, MYEMU2_R12);\n"
        "      if (cfun->machine->callee_saved_reg_size <= 255)\n"
        "\t{\n"
        "\t  emit_move_insn (reg, hard_frame_pointer_rtx);\n"
        "\t  emit_insn (gen_subsi3\n"
        "\t\t     (reg, reg,\n"
        "\t\t      GEN_INT (cfun->machine->callee_saved_reg_size)));\n"
        "\t}\n"
        "      else\n"
        "\t{\n"
        "\t  emit_move_insn (reg,\n"
        "\t\t\t  GEN_INT (-cfun->machine->callee_saved_reg_size));\n"
        "\t  emit_insn (gen_addsi3 (reg, reg, hard_frame_pointer_rtx));\n"
        "\t}\n"
        "      for (regno = FIRST_PSEUDO_REGISTER; regno-- > 0; )\n"
        "\tif (!call_used_or_fixed_reg_p (regno)\n"
        "\t    && df_regs_ever_live_p (regno))\n"
        "\t  {\n"
        "\t    rtx preg = gen_rtx_REG (Pmode, regno);\n"
        "\t    emit_insn (gen_movsi_pop (reg, preg));\n"
        "\t  }\n"
        "    }\n"
        "\n"
        "  /* Restore the caller's frame pointer, then release this function's\n"
        "     complete frame.  The saved frame pointer is not the caller's SP: the\n"
        "     latter is the incoming SP plus both the local/outgoing area and any\n"
        "     callee-save pushes. */\n"
        "  emit_move_insn (hard_frame_pointer_rtx,\n"
        "                  gen_rtx_MEM (SImode, plus_constant (Pmode, stack_pointer_rtx,\n"
        "                                             4)));\n"
        "  {\n"
        "    HOST_WIDE_INT frame_release =\n"
        "      cfun->machine->size_for_adjusting_sp\n"
        "      + cfun->machine->callee_saved_reg_size;\n"
        "    if (frame_release <= 2047)\n"
        "      emit_insn (gen_addsi3 (stack_pointer_rtx, stack_pointer_rtx,\n"
        "                             GEN_INT (frame_release)));\n"
        "    else\n"
        "      {\n"
        "        rtx scratch = gen_rtx_REG (SImode, MYEMU2_R12);\n"
        "        emit_move_insn (scratch, GEN_INT (frame_release));\n"
        "        emit_insn (gen_addsi3 (stack_pointer_rtx, stack_pointer_rtx, scratch));\n"
        "      }\n"
        "  }\n"
        "  emit_jump_insn (gen_returner ());\n"
        "}"
    )
    new_epilogue = (
        "void\n"
        "myemulator2_expand_epilogue (void)\n"
        "{\n"
        "  int regno;\n"
        "  rtx reg;\n"
        "  HOST_WIDE_INT header_offset =\n"
        "    cfun->machine->size_for_adjusting_sp\n"
        "    + cfun->machine->callee_saved_reg_size\n"
        "    - (crtl->outgoing_args_size + 32);\n"
        "\n"
        "  /* The header was written at the final SP, below the incoming\n"
        "     frame pointer and any callee-save pushes.  Compute that address\n"
        "     without using R12 until the saved LR and caller FP are loaded. */\n"
        "  reg = gen_rtx_REG (Pmode, MYEMU2_R12);\n"
        "  emit_move_insn (reg, hard_frame_pointer_rtx);\n"
        "  while (header_offset > 255)\n"
        "    {\n"
        "      emit_insn (gen_subsi3 (reg, reg, GEN_INT (255)));\n"
        "      header_offset -= 255;\n"
        "    }\n"
        "  if (header_offset != 0)\n"
        "    emit_insn (gen_subsi3 (reg, reg, GEN_INT (header_offset)));\n"
        "  emit_move_insn (gen_rtx_REG (SImode, MYEMU2_LR),\n"
        "                  gen_rtx_MEM (SImode, reg));\n"
        "  {\n"
        "    rtx caller_fp = gen_rtx_REG (Pmode, MYEMU2_R11);\n"
        "    emit_move_insn (caller_fp,\n"
        "                    gen_rtx_MEM (SImode,\n"
        "                                 plus_constant (Pmode, reg, 4)));\n"
        "\n"
        "    /* Callee-saved registers were pushed immediately below the\n"
        "       incoming FP, so restore them from FP - saved-size. */\n"
        "    if (cfun->machine->callee_saved_reg_size != 0)\n"
        "      {\n"
        "        reg = gen_rtx_REG (Pmode, MYEMU2_R12);\n"
        "        emit_move_insn (reg, hard_frame_pointer_rtx);\n"
        "        HOST_WIDE_INT saved = cfun->machine->callee_saved_reg_size;\n"
        "        while (saved > 255)\n"
        "          {\n"
        "            emit_insn (gen_subsi3 (reg, reg, GEN_INT (255)));\n"
            "            saved -= 255;\n"
        "          }\n"
        "        if (saved != 0)\n"
        "          emit_insn (gen_subsi3 (reg, reg, GEN_INT (saved)));\n"
        "        for (regno = FIRST_PSEUDO_REGISTER; regno-- > 0; )\n"
        "\tif (!call_used_or_fixed_reg_p (regno)\n"
        "\t    && df_regs_ever_live_p (regno))\n"
        "\t  {\n"
        "\t    rtx preg = gen_rtx_REG (Pmode, regno);\n"
        "\t    emit_insn (gen_movsi_pop (reg, preg));\n"
        "\t  }\n"
        "      }\n"
        "\n"
        "    /* Restore SP to the incoming value.  This also discards the\n"
        "       complete fixed frame and any dynamic alloca/VLA area. */\n"
        "    emit_move_insn (stack_pointer_rtx, hard_frame_pointer_rtx);\n"
        "    emit_move_insn (hard_frame_pointer_rtx, caller_fp);\n"
        "  }\n"
        "  emit_jump_insn (gen_returner ());\n"
        "}"
    )
    if old_epilogue not in text:
        raise SystemExit("could not locate myemulator2_expand_epilogue to apply the alloca/VLA stack-restore fix")
    text = text.replace(old_epilogue, new_epilogue, 1)

    # BUG FIX ("Bug #6"): alloca()/VLA followed by a call could corrupt
    # either the caller's saved LR/frame-pointer slot or a subsequent
    # callee's own return value.  Root cause: this port's calling
    # convention has every callee spill its own incoming register
    # arguments to "home slots" at [own FP + STACK_POINTER_OFFSET,
    # own FP + STACK_POINTER_OFFSET + REG_PARM_STACK_SPACE) as ordinary
    # prologue codegen (musl's own strcpy/memcpy do this unconditionally).
    # This is always safe for an ordinary call site because the real SP
    # is constant for the whole function body and the fixed frame already
    # reserves REG_PARM_STACK_SPACE bytes above it for exactly this
    # purpose.  alloca()/a VLA moves the real SP itself, so a call made
    # afterwards uses that lower SP as its own incoming FP, with nothing
    # above it reserved.  The fix has two required parts, both needed
    # together (an earlier single-part fix regressed previously-working
    # cases -- see the derivation in myemulator2.h/.cc):
    #   1. STACK_DYNAMIC_OFFSET (myemulator2_stack_dynamic_offset() below)
    #      returns STACK_POINTER_OFFSET + REG_PARM_STACK_SPACE, so
    #      alloca()'s returned pointer clears the *entire* register-
    #      argument home area a subsequent callee might spill into
    #      (returning only STACK_POINTER_OFFSET is NOT sufficient -- it
    #      places the pointer at the very first byte of that area).
    #   2. STACK_DYNAMIC_PAD (myemulator2.h, and the matching generic GCC
    #      support in gcc/defaults.h + gcc/builtins.cc patched below) pads
    #      the actual alloca() reservation by the same amount, since
    #      GCC's own get_dynamic_stack_size() (explow.cc) never grows the
    #      requested size to account for a nonzero STACK_DYNAMIC_OFFSET on
    #      its own -- without this, part 1 alone only moves where the
    #      returned pointer sits *within* the same, unchanged region,
    #      which regressed cases where the caller's own data then ran
    #      into the fixed frame above it.
    # myemulator2.h/myemulator2-protos.h already carry the finished
    # STACK_DYNAMIC_OFFSET/STACK_DYNAMIC_PAD macros and prototype (copied
    # verbatim above), so only the myemulator2.cc function body and the
    # two generic GCC files need patching here.
    dynamic_offset_marker = "void\nmyemulator2_expand_prologue (void)"
    dynamic_offset_fn = (
        "HOST_WIDE_INT\n"
        "myemulator2_stack_dynamic_offset (void)\n"
        "{\n"
        "  myemulator2_compute_frame ();\n"
        "  /* Dynamic allocations must sit above the whole outgoing argument block\n"
        "     (register home area plus any stack-passed arguments), otherwise a\n"
        "     call with more than four argument words overwrites alloca/VLA data.  */\n"
        "  HOST_WIDE_INT outgoing = (HOST_WIDE_INT) crtl->outgoing_args_size;\n"
        "  if (outgoing < REG_PARM_STACK_SPACE (NULL_TREE))\n"
        "    outgoing = REG_PARM_STACK_SPACE (NULL_TREE);\n"
        "  return STACK_POINTER_OFFSET + outgoing;\n"
        "}\n"
        "\n"
        + dynamic_offset_marker
    )
    if dynamic_offset_marker in text and "myemulator2_stack_dynamic_offset (void)\n{" not in text:
        text = text.replace(dynamic_offset_marker, dynamic_offset_fn, 1)

    text = text.replace('  return STACK_POINTER_OFFSET + REG_PARM_STACK_SPACE (NULL_TREE);\n', '  /* Dynamic allocations must sit above the whole outgoing argument block\n     (register home area plus any stack-passed arguments), otherwise a\n     call with more than four argument words overwrites alloca/VLA data.  */\n  HOST_WIDE_INT outgoing = (HOST_WIDE_INT) crtl->outgoing_args_size;\n  if (outgoing < REG_PARM_STACK_SPACE (NULL_TREE))\n    outgoing = REG_PARM_STACK_SPACE (NULL_TREE);\n  return STACK_POINTER_OFFSET + outgoing;\n')
    # The SP must stay word-aligned after every instruction: an interrupt
    # taken between two SP steps (e.g. 255 + 1) finds a misaligned SSP and the
    # CPU raises a double fault.  Step large frames by 252 instead.
    sp_step_old = ("\t  while (i > 255)\n\t    {\n"
                   "\t      insn = emit_insn (gen_subsi3 (stack_pointer_rtx,\n"
                   "\t\t\t\t\t    stack_pointer_rtx,\n"
                   "\t\t\t\t\t    GEN_INT (255)));\n"
                   "\t      RTX_FRAME_RELATED_P (insn) = 1;\n"
                   "\t      i -= 255;\n")
    text = text.replace(sp_step_old, sp_step_old.replace("255", "252"))
    mid_old = ("      while ((i >= 255) && (i <= 510))\n\t{\n"
               "\t  insn = emit_insn (gen_subsi3 (stack_pointer_rtx,\n"
               "\t\t\t\t\tstack_pointer_rtx,\n"
               "\t\t\t\t\tGEN_INT (255)));\n"
               "\t  RTX_FRAME_RELATED_P (insn) = 1;\n"
               "\t  i -= 255;\n")
    mid_new = (mid_old.replace("(i >= 255)", "(i > 255)")
               .replace("GEN_INT (255)", "GEN_INT (252)")
               .replace("i -= 255", "i -= 252"))
    if mid_old in text:
        text = text.replace(mid_old, mid_new, 1)
    elif mid_new not in text:
        raise SystemExit("could not locate mid-size frame allocation loop")
    cc.write_text(text)

    # Part 2 of the Bug #6 fix: teach generic GCC about STACK_DYNAMIC_PAD.
    # gcc/defaults.h gets a target-opt-in default of 0 (a no-op for every
    # other GCC target); gcc/builtins.cc's expand_builtin_alloca() grows
    # the requested alloca() size by STACK_DYNAMIC_PAD bytes before
    # rounding/alignment, when nonzero.
    defaults_h = root / "gcc/defaults.h"
    defaults_text = defaults_h.read_text()
    if "STACK_DYNAMIC_PAD" not in defaults_text:
        marker = "#ifndef STACK_POINTER_OFFSET\n#define STACK_POINTER_OFFSET    0\n#endif\n"
        if marker not in defaults_text:
            raise SystemExit("could not locate STACK_POINTER_OFFSET default in gcc/defaults.h")
        addition = (
            "\n/* Extra bytes a target wants padded onto every alloca()/VLA request\n"
            "   *before* rounding/alignment, in addition to what the requester asked\n"
            "   for.  This exists for targets whose STACK_DYNAMIC_OFFSET returns a\n"
            "   nonzero value to reserve a callee-scratch cushion above the pointer\n"
            "   alloca() returns: get_dynamic_stack_size() only pads for alignment,\n"
            "   never for STACK_DYNAMIC_OFFSET's own margin, so simply returning a\n"
            "   nonzero STACK_DYNAMIC_OFFSET does not actually reserve any additional\n"
            "   stack space -- it only changes where within the *unchanged* allocation\n"
            "   the returned pointer sits, which can put the tail of a full-sized\n"
            "   request outside the allocated region.  Defaults to 0, which preserves\n"
            "   the exact previous behavior on every other target.  */\n"
            "#ifndef STACK_DYNAMIC_PAD\n"
            "#define STACK_DYNAMIC_PAD    0\n"
            "#endif\n"
        )
        defaults_text = defaults_text.replace(marker, marker + addition, 1)
        defaults_h.write_text(defaults_text)

    builtins_cc = root / "gcc/builtins.cc"
    builtins_text = builtins_cc.read_text()
    if "STACK_DYNAMIC_PAD" not in builtins_text:
        marker = (
            "  /* Compute the argument.  */\n"
            "  op0 = expand_normal (CALL_EXPR_ARG (exp, 0));\n"
        )
        if marker not in builtins_text:
            raise SystemExit("could not locate expand_builtin_alloca's argument computation in gcc/builtins.cc")
        addition = (
            "\n"
            "  /* Some targets return a nonzero STACK_DYNAMIC_OFFSET so that alloca()'s\n"
            "     returned pointer sits above a mandatory callee-scratch cushion rather\n"
            "     than at the very bottom of the region anti_adjust_stack() reserves.\n"
            "     get_dynamic_stack_size() never pads SIZE to account for that margin\n"
            "     (it only pads for alignment), so without also growing the actual\n"
            "     reservation here, the requester's own data can extend past the end\n"
            "     of the (unchanged) allocation and into whatever sits directly above\n"
            "     it.  STACK_DYNAMIC_PAD lets such a target grow the real reservation\n"
            "     to match; it is 0 (a no-op) everywhere else.  */\n"
            "  if (STACK_DYNAMIC_PAD > 0)\n"
            "    {\n"
            "      op0 = convert_to_mode (Pmode, op0, 1);\n"
            "      op0 = plus_constant (Pmode, op0, STACK_DYNAMIC_PAD);\n"
            "      op0 = force_operand (op0, NULL_RTX);\n"
            "    }\n"
        )
        builtins_text = builtins_text.replace(marker, marker + addition, 1)
        builtins_cc.write_text(builtins_text)


    config_gcc = root / "gcc/config.gcc"
    text = config_gcc.read_text()
    # Remove any stale insertion from older script revisions before placing
    # the hosted-only define in the correct target case.
    text = text.replace('\n\ttm_defines="MYEMU2_HOSTED_LINUX=1"', '')
    text = text.replace("moxie*)\tcpu_type=moxie", "myemulator2*)\tcpu_type=myemulator2\n\ttarget_has_targetm_common=no\n\t;;\nmoxie*)\tcpu_type=moxie")
    if "myemulator2-*-linux-musl*)" not in text:
        linux_case = (
            "myemulator2-*-linux-musl*)\n"
            "\tgas=yes\n"
            "\tgnu_ld=yes\n"
            "\ttm_file=\"elfos.h gnu-user.h linux.h glibc-stdint.h ${tm_file} myemulator2/myemulator2-linux.h\"\n"
            "\ttmake_file=\"${tmake_file} myemulator2/t-myemulator2-linux t-softfp-sfdf t-softfp t-linux\"\n"
            "\t;;\n")
        marker = "moxie-*-elf)\n"
        if marker not in text:
            raise SystemExit("could not locate GCC moxie ELF target case")
        text = text.replace(marker, linux_case + marker, 1)
    else:
        text = text.replace(
            'tm_file="elfos.h gnu-user.h linux.h glibc-stdint.h ${tm_file}"',
            'tm_file="elfos.h gnu-user.h linux.h glibc-stdint.h ${tm_file} myemulator2/myemulator2-linux.h"',
            1)
        text = text.replace(
            'tmake_file="${tmake_file} myemulator2/t-myemulator2"',
            'tmake_file="${tmake_file} myemulator2/t-myemulator2-linux t-softfp-sfdf t-softfp t-linux"',
            1)
    config_gcc.write_text(text)

    # GCC bundles additional Autoconf projects (notably gettext) with their
    # own config.sub copies.  A Canadian build uses those copies to validate
    # the host triplet, so teach every bundled copy about MyEmulator2 rather
    # than only patching GCC's top-level script.
    marker = "\t\t\t| moxie " + "\\" + "\n"
    replacement = "\t\t\t| myemulator2 " + "\\" + "\n" + marker
    patched_config_sub = 0
    for config_sub in root.rglob("config.sub"):
        text = config_sub.read_text()
        if "myemulator2" in text:
            continue
        if marker in text:
            config_sub.write_text(text.replace(marker, replacement, 1))
            patched_config_sub += 1
            continue
        # Older bundled Autoconf projects use a compact, unaligned case
        # list.  They are still used by Canadian builds, so accept that
        # spelling as well instead of silently leaving a stale config.sub.
        compact = re.compile(r"(?m)^(\s*\|\s*)moxie(\s*\\\s*)$")
        updated, count = compact.subn(r"\1myemulator2\2\n\g<0>", text, count=1)
        if count:
            config_sub.write_text(updated)
            patched_config_sub += 1
    if not patched_config_sub and "myemulator2" not in (root / "config.sub").read_text():
        raise SystemExit("could not find config.sub machine-name list")

    host = root / "libgcc/config.host"
    text = host.read_text()
    text = text.replace(
        "myemulator2-*-linux-musl*)\n",
        "myemulator2-*-linux-musl* | myemulator2-linux-musl*)\n",
        1)
    text = text.replace(
        '\ttmake_file="$tmake_file myemulator2/t-myemulator2"\n\textra_parts="$extra_parts crti.o crtn.o crtbegin.o crtend.o"',
        '\ttmake_file="$tmake_file myemulator2/t-myemulator2-linux t-softfp-sfdf t-softfp"\n\textra_parts="$extra_parts crti.o crtn.o crtbegin.o crtend.o"',
        1)
    if not re.search(r"myemulator2-\*-linux-musl", text):
        text = text.replace(
            "myemulator2-*-elf)\n",
            "myemulator2-*-linux-musl* | myemulator2-linux-musl*)\n"
            "\ttmake_file=\"$tmake_file myemulator2/t-myemulator2\"\n"
            "\textra_parts=\"$extra_parts crti.o crtn.o crtbegin.o crtend.o\"\n"
            "\t;;\nmyemulator2-*-elf)\n",
            1)
    if "myemulator2*) cpu_type=myemulator2" not in text:
        marker = "moxie*)"
        if marker not in text:
            raise SystemExit("could not find libgcc moxie cpu_type case")
        text = text.replace(marker, "myemulator2*) cpu_type=myemulator2\n\t;;\n" + marker, 1)
    if "myemulator2-*-linux-musl" not in text:
        text = text.replace("moxie-*-elf | moxie-*-moxiebox* | moxie-*-uclinux* | moxie-*-rtems*)",
                            "myemulator2-*-linux-musl | myemulator2-*-elf | moxie-*-elf | moxie-*-moxiebox* | moxie-*-uclinux* | moxie-*-rtems*)")
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
    shutil.copyfile(fragment / "t-myemulator2-linux", libgcc_target / "t-myemulator2-linux")
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
# Hosted MyEmulator2 Linux uses GCC's generic DWARF2 unwinder.  The
# freestanding target has no exception ABI, but this Linux fragment must not
# suppress libgcc's EH objects: native GCC and libstdc++ are C++ programs.
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
