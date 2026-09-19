#!/usr/bin/env python3
"""Prepare an external binutils 2.46.0 tree for the MyEmulator2 port.

The upstream tree is never stored in this repository.  The target-specific
files are generated in the ignored build tree from the small, maintained
source fragments in toolchain/binutils.
"""
from pathlib import Path
import shutil
import sys


def replace(path: Path, old: str, new: str) -> None:
    text = path.read_text()
    path.write_text(text.replace(old, new))


def add_after(path: Path, needle: str, addition: str) -> None:
    text = path.read_text()
    if addition.strip() in text:
        return
    if needle not in text:
        raise SystemExit(f"cannot patch {path}: missing {needle!r}")
    path.write_text(text.replace(needle, needle + addition, 1))


def add_before(path: Path, needle: str, addition: str) -> None:
    text = path.read_text()
    if addition.strip() in text:
        return
    if needle not in text:
        raise SystemExit(f"cannot patch {path}: missing {needle!r}")
    path.write_text(text.replace(needle, addition + needle, 1))


def copy_target(root: Path, subdir: str, names: list[str]) -> None:
    for name in names:
        src = root / subdir / name.replace("myemulator2", "moxie")
        dst = root / subdir / name
        shutil.copyfile(src, dst)
        replace(dst, "MOXIE", "MYEMULATOR2")
        replace(dst, "Moxie", "MyEmulator2")
        replace(dst, "moxie", "myemulator2")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: prepare-binutils-source.py BINUTILS_SOURCE")
    root = Path(sys.argv[1]).resolve()

    copy_target(root, "bfd", ["cpu-myemulator2.c", "elf32-myemulator2.c"])
    copy_target(root, "include/elf", ["myemulator2.h"])
    copy_target(root, "include/opcode", ["myemulator2.h"])
    copy_target(root, "gas/config", ["tc-myemulator2.c", "tc-myemulator2.h"])
    copy_target(root, "opcodes", ["myemulator2-dis.c", "myemulator2-opc.c"])
    copy_target(root, "ld/emulparams", ["elf32myemulator2.sh"])
    (root / "ld/scripttempl").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(Path(__file__).resolve().parents[1] /
                    "binutils/ld/scripttempl/elf32myemulator2.sc",
                    root / "ld/scripttempl/elf32myemulator2.sc")

    # The generated source initially inherits the regular GNU target plumbing
    # from Moxie.  The maintained fragments replace its instruction and ELF
    # definitions; these small table edits make configure/build discover them.
    for makefile in (root / "bfd/Makefile.am", root / "bfd/Makefile.in"):
        replace(makefile, "cpu-moxie", "cpu-moxie cpu-myemulator2")
        replace(makefile, "elf32-moxie", "elf32-moxie elf32-myemulator2")
    for configure in (root / "bfd/configure.ac", root / "bfd/configure"):
        add_after(configure,
                  "    moxie_elf32_le_vec)\t\t tb=\"$tb elf32-moxie.lo elf32.lo $elf\" ;;",
                  "\n    myemulator2_elf32_be_vec)\t tb=\"$tb elf32-myemulator2.lo elf32.lo $elf\" ;;\n"
                  "    myemulator2_elf32_le_vec)\t tb=\"$tb elf32-myemulator2.lo elf32.lo $elf\" ;;")
    for targets in (root / "bfd/targets.c",):
        add_after(targets, "extern const bfd_target moxie_elf32_le_vec;",
                  "\nextern const bfd_target myemulator2_elf32_be_vec;\n"
                  "extern const bfd_target myemulator2_elf32_le_vec;")
        add_after(targets, "\t&moxie_elf32_le_vec,",
                  "\n\t&myemulator2_elf32_be_vec,\n\t&myemulator2_elf32_le_vec,")
    add_after(root / "bfd/archures.c",
              "extern const bfd_arch_info_type bfd_moxie_arch;",
              "\nextern const bfd_arch_info_type bfd_myemulator2_arch;")
    add_after(root / "bfd/archures.c", "    &bfd_moxie_arch,",
              "\n    &bfd_myemulator2_arch,")
    for makefile in (root / "opcodes/Makefile.am", root / "opcodes/Makefile.in"):
        replace(makefile, "moxie-dis.c", "moxie-dis.c myemulator2-dis.c")
        replace(makefile, "moxie-opc.c", "moxie-opc.c myemulator2-opc.c")
    for makefile in (root / "gas/Makefile.am", root / "gas/Makefile.in"):
        replace(makefile, "config/tc-moxie.c", "config/tc-moxie.c config/tc-myemulator2.c")
        replace(makefile, "config/tc-moxie.h", "config/tc-moxie.h config/tc-myemulator2.h")
    for makefile in (root / "ld/Makefile.am", root / "ld/Makefile.in"):
        replace(makefile, "eelf32moxie.c", "eelf32moxie.c eelf32myemulator2.c")

    add_before(root / "bfd/config.bfd",
               "  moxie-*-elf | moxie-*-rtems* | moxie-*-uclinux)",
               "  myemulator2-*-elf)\n    targ_defvec=myemulator2_elf32_le_vec\n    ;;\n\n")
    add_after(root / "gas/configure.tgt", "  moxie-*-*)\t\t\t\tfmt=elf ;;\n",
              "  myemulator2-*-*)\t\t\t\tfmt=elf endian=little ;;\n")
    add_after(root / "ld/configure.tgt", "moxie-*-*)\t\ttarg_emul=elf32moxie\n\t\t\t;;\n",
              "myemulator2-*-*)\t\ttarg_emul=elf32myemulator2\n\t\t\t;;\n")
    for configure in (root / "opcodes/configure.ac", root / "opcodes/configure"):
        add_after(configure,
                  "\tbfd_moxie_arch)\t\tta=\"$ta moxie-dis.lo moxie-opc.lo\" ;;",
                  "\n\tbfd_myemulator2_arch)\t\tta=\"$ta myemulator2-dis.lo myemulator2-opc.lo\" ;;")

    # opcodes/disassemble.c is the common objdump dispatcher.  The generated
    # architecture list does not discover a newly added disassembler by
    # itself, so register the target explicitly here.
    disassemble = root / "opcodes/disassemble.c"
    add_after(disassemble, "#define ARCH_moxie\n",
              "#define ARCH_myemulator2\n")
    add_after(disassemble, "#ifdef ARCH_moxie\n    case bfd_arch_moxie:\n      disassemble = print_insn_moxie;\n      break;\n#endif\n",
              "#ifdef ARCH_myemulator2\n    case bfd_arch_myemulator2:\n      disassemble = print_insn_myemulator2;\n      break;\n#endif\n")
    add_after(root / "opcodes/disassemble.h",
              "extern int print_insn_moxie\t\t(bfd_vma, disassemble_info *);\n",
              "extern int print_insn_myemulator2\t(bfd_vma, disassemble_info *);\n")

    # readelf has a small hand-written relocation-name dispatch table rather
    # than consulting BFD at runtime.  Add the project-local ELF header and
    # all three places where it dispatches machine-specific relocations.
    readelf = root / "binutils/readelf.c"
    add_after(readelf, '#include "elf/moxie.h"\n',
              '#include "elf/myemulator2.h"\n')
    add_after(readelf,
              "\tcase EM_MOXIE:\n\t  rtype = elf_moxie_reloc_type (type);\n\t  break;\n",
              "\n\tcase EM_MYEMULATOR2:\n\t  rtype = elf_myemulator2_reloc_type (type);\n\t  break;\n")
    add_after(readelf,
              "    case EM_MOXIE:              return \"Moxie\";\n",
              "    case EM_MYEMULATOR2:        return \"MyEmulator2\";\n")
    add_after(readelf,
              "    case EM_MOXIE:\n      return reloc_type == 1; /* R_MOXIE_32.  */\n",
              "    case EM_MYEMULATOR2:\n      return reloc_type == 1; /* R_MYEMULATOR2_32. */\n")
    add_after(readelf,
              "    case EM_MOXIE:   /* R_MOXIE_NONE.  */\n",
              "    case EM_MYEMULATOR2: /* R_MYEMULATOR2_NONE. */\n")

    add_after(root / "bfd/archures.c",
              ".  bfd_arch_moxie,     {* The moxie processor.  *}",
              "\n.  bfd_arch_myemulator2,   {* MyEmulator2 32-bit architecture.  *}")
    add_after(root / "bfd/archures.c", ".#define bfd_mach_moxie\t\t1",
              "\n.#define bfd_mach_myemulator2\t1")
    add_after(root / "bfd/bfd-in2.h",
              "  bfd_arch_moxie,     /* The moxie processor.  */",
              "\n  bfd_arch_myemulator2, /* MyEmulator2 32-bit architecture. */")
    add_after(root / "bfd/bfd-in2.h", "#define bfd_mach_moxie         1",
              "\n#define bfd_mach_myemulator2\t1")

    # The target triplet is intentionally accepted by config.sub.  This is a
    # project-local target name, not an alias for another CPU.
    config_sub = root / "config.sub"
    text = config_sub.read_text()
    if "myemulator2" not in text:
        text = text.replace("\t\t\t| moxie \\",
                            "\t\t\t| myemulator2 \\\n\t\t\t| moxie \\")
        text = text.replace("| moxiebox*", "| myemulator2* | moxiebox*")
        config_sub.write_text(text)

    # Use the maintained target fragments after the generated copy step.
    fragment_root = Path(__file__).resolve().parents[1] / "binutils"
    for rel in (
        "bfd/cpu-myemulator2.c", "bfd/elf32-myemulator2.c",
        "include/elf/myemulator2.h", "include/opcode/myemulator2.h",
        "gas/config/tc-myemulator2.c", "gas/config/tc-myemulator2.h",
        "opcodes/myemulator2-dis.c", "opcodes/myemulator2-opc.c",
        "ld/emulparams/elf32myemulator2.sh",
        "ld/scripttempl/elf32myemulator2.sc",
    ):
        source = fragment_root / rel
        if source.exists():
            destination = root / rel
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)

    elf_backend = root / "bfd/elf32-myemulator2.c"
    replace(elf_backend, "#define ELF_MACHINE_ALT1\tEM_MYEMULATOR2_OLD\n", "")
    replace(elf_backend, "BFD_RELOC_MYEMULATOR2_10_PCREL", "BFD_RELOC_MOXIE_10_PCREL")
    elf_header = root / "include/elf/myemulator2.h"
    add_before(elf_header, "#include \"elf/reloc-macros.h\"",
               "#define EM_MYEMULATOR2 0xF2E2\n\n")
    backend = elf_backend.read_text()
    # MyEmulator2 uses 4 KiB virtual pages.  The generated backend starts
    # from Moxie's byte-granular setting, which produces PT_LOAD segments
    # whose file offset and virtual address are not page-congruent.  Linux
    # ELF loading requires the ABI page congruence rule.
    backend = backend.replace("#define ELF_MAXPAGESIZE\t\t0x1",
                              "#define ELF_MAXPAGESIZE\t\t0x1000")
    backend = backend.replace("R_MYEMULATOR2_PCREL10", "R_MYEMULATOR2_BRANCH13")
    backend = backend.replace("10 bit PC-relative", "13 bit PC-relative")
    backend = backend.replace("\t 10,", "\t 13,", 1)
    backend = backend.replace("0x000003FF", "0x00001FFF")
    backend = backend.replace(
        "};\n\f\n/* Map BFD reloc types",
        """  HOWTO (R_MYEMULATOR2_JUMP26, 0, 2, 26, true, 0,
   complain_overflow_signed, bfd_elf_generic_reloc,
   \"R_MYEMULATOR2_JUMP26\", false, 0, 0x03ffffff, true),
  HOWTO (R_MYEMULATOR2_HI20, 0, 2, 20, false, 1,
   complain_overflow_bitfield, bfd_elf_generic_reloc,
   \"R_MYEMULATOR2_HI20\", false, 0, 0x001ffffe, false),
  HOWTO (R_MYEMULATOR2_LO12, 0, 2, 12, false, 0,
   complain_overflow_bitfield, bfd_elf_generic_reloc,
   \"R_MYEMULATOR2_LO12\", false, 0, 0x00000fff, false),
};
\\f
/* Map BFD reloc types""")
    backend = backend.replace(
        "{ BFD_RELOC_MOXIE_10_PCREL,  R_MYEMULATOR2_BRANCH13 },",
        "{ BFD_RELOC_32_PCREL,  R_MYEMULATOR2_BRANCH13 },\n"
        "  { BFD_RELOC_26,  R_MYEMULATOR2_JUMP26 },\n"
        "  { BFD_RELOC_HI16_S,  R_MYEMULATOR2_HI20 },\n"
        "  { BFD_RELOC_LO16,  R_MYEMULATOR2_LO12 },")
    old_switch = """  switch (howto->type)
    {
    default:
      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
\t\t\t\t    contents, rel->r_offset,
\t\t\t\t    relocation, rel->r_addend);
    }

  return r;"""
    new_switch = """  bfd_vma place = input_section->output_section->vma
    + input_section->output_offset + rel->r_offset;
  bfd_vma value = relocation + rel->r_addend;
  uint32_t word;
  switch (howto->type)
    {
    case R_MYEMULATOR2_BRANCH13:
      value = (bfd_vma)((bfd_signed_vma)value - (bfd_signed_vma)(place + 4));
      if ((bfd_signed_vma)value % 4 != 0 || (bfd_signed_vma)value / 4 < -4096
          || (bfd_signed_vma)value / 4 > 4095) return bfd_reloc_overflow;
      word = bfd_getl32(contents + rel->r_offset);
      bfd_putl32((word & ~0x1fffU) | ((value / 4) & 0x1fffU),
                 contents + rel->r_offset);
      return bfd_reloc_ok;
    case R_MYEMULATOR2_JUMP26:
      value = (bfd_vma)((bfd_signed_vma)value - (bfd_signed_vma)(place + 4));
      if ((bfd_signed_vma)value % 4 != 0 || (bfd_signed_vma)value / 4 < -33554432
          || (bfd_signed_vma)value / 4 > 33554431) return bfd_reloc_overflow;
      word = bfd_getl32(contents + rel->r_offset);
      bfd_putl32((word & ~0x03ffffffU) | ((value / 4) & 0x03ffffffU),
                 contents + rel->r_offset);
      return bfd_reloc_ok;
    case R_MYEMULATOR2_HI20:
      word = bfd_getl32(contents + rel->r_offset);
      bfd_putl32((word & ~0x001ffffeU) | (((value >> 12) & 0xfffffU) << 1),
                 contents + rel->r_offset);
      return bfd_reloc_ok;
    case R_MYEMULATOR2_LO12:
      word = bfd_getl32(contents + rel->r_offset);
      bfd_putl32((word & ~0xfffU) | (value & 0xfffU), contents + rel->r_offset);
      return bfd_reloc_ok;
    default:
      return _bfd_final_link_relocate (howto, input_bfd, input_section,
                                       contents, rel->r_offset, relocation,
                                       rel->r_addend);
    }"""
    if old_switch not in backend:
        raise SystemExit("cannot patch MyEmulator2 relocation switch")
    backend = backend.replace(old_switch, new_switch)
    backend = backend.replace("\\f", "\f")
    elf_backend.write_text(backend)


if __name__ == "__main__":
    main()
