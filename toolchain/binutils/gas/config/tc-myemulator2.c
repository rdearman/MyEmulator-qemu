#include "as.h"
#include "safe-ctype.h"
#include "opcode/myemulator2.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#endif
#include "elf/myemulator2.h"

const char comment_chars[] = "#";
const char line_separator_chars[] = ";";
const char line_comment_chars[] = "#";
const char FLT_CHARS[] = "rRsSfFdDxXpP";
const char EXP_CHARS[] = "eE";
const char md_shortopts[] = "";
const struct option md_longopts[] = { { 0, 0, 0, 0 } };
const size_t md_longopts_size = sizeof (md_longopts);
static void s_myemulator2_rodata (int ignored ATTRIBUTE_UNUSED)
{
  subseg_new (".rodata", 0);
}
const pseudo_typeS md_pseudo_table[] = {
  { "word", cons, 4 },
  { "rodata", s_myemulator2_rodata, 0 },
  { 0, 0, 0 }
};
extern int target_big_endian;

static htab_t opcode_hash_control;

static char *trim(char *s)
{
  while (is_whitespace(*s)) s++;
  char *e = s + strlen(s);
  while (e > s && is_whitespace(e[-1])) --e;
  *e = 0;
  return s;
}

static int regno(const char *s)
{
  if (!strcasecmp(s, "zero")) return 0;
  if (!strcasecmp(s, "sp")) return 13;
  if (!strcasecmp(s, "lr")) return 14;
  if ((s[0] == 'r' || s[0] == 'R') && s[1] && !s[2])
    return s[1] >= '0' && s[1] <= '9' ? s[1] - '0' : -1;
  if ((s[0] == 'r' || s[0] == 'R') && s[1] >= '1' && s[1] <= '9'
      && s[2] >= '0' && s[2] <= '9' && !s[3]) {
    int n = (s[1] - '0') * 10 + s[2] - '0';
    return n < 16 ? n : -1;
  }
  return -1;
}

static int sysreg(const char *s)
{
  static const char *const names[] = {
    "sr", "usp", "ssp", "vbr", "ptbr", "mmcr", "time_lo", "time_hi",
    "timecmp_lo", "timecmp_hi", "tp", "dfsp"
  };
  for (unsigned i = 0; i < ARRAY_SIZE(names); i++)
    if (!strcasecmp(s, names[i])) return i;
  if (!strcasecmp(s, "time")) return MYEMU2_TIME_LO;
  if (!strcasecmp(s, "timecmp")) return MYEMU2_TIMECMP_LO;
  return -1;
}

static bool parse_expr(char *text, expressionS *out)
{
  char *save = input_line_pointer;
  input_line_pointer = text;
  expression(out);
  input_line_pointer = save;
  return out->X_op != O_illegal;
}

/* Parse the ABI's explicit relocation modifiers.  Keeping the modifier
   outside GAS's ordinary expression parser is intentional: %hi/%lo are
   target relocation operators, not C-like arithmetic operators. */
static bool parse_reloc_expr(char *text, expressionS *out,
                             bfd_reloc_code_real_type *reloc)
{
  char *s = trim(text);
  size_t length = strlen(s);
  *reloc = BFD_RELOC_NONE;
  if ((length > 5 && !strncasecmp(s, "%hi(", 4) && s[length - 1] == ')')
      || (length > 5 && !strncasecmp(s, "%lo(", 4) && s[length - 1] == ')'))
    {
      bool hi = !strncasecmp(s, "%hi(", 4);
      s[length - 1] = 0;
      s += 4;
      *reloc = hi ? BFD_RELOC_HI16_S : BFD_RELOC_LO16;
    }
  return parse_expr(s, out);
}

static char **args(char *s, int *count)
{
  char **v = XNEWVEC(char *, 8);
  int n = 0;
  for (char *p = strtok(s, ","); p && n < 8; p = strtok(NULL, ","))
    v[n++] = trim(p);
  *count = n;
  return v;
}

static void word(uint32_t value)
{
  char *p = frag_more(4);
  md_number_to_chars(p, value, 4);
}

static void fixword(uint32_t value, expressionS *e, bool pcrel,
                    bfd_reloc_code_real_type reloc)
{
  char *p = frag_more(4);
  md_number_to_chars(p, value, 4);
  fix_new_exp(frag_now, p - frag_now->fr_literal, 4, e, pcrel, reloc);
}

static void bad(const char *message)
{
  as_bad("%s", message);
}

static int immediate(char *s, expressionS *e)
{
  if (!parse_expr(s, e)) { bad(_("invalid expression")); return 0; }
  if (e->X_op == O_constant) return (int)e->X_add_number;
  return 0;
}

static bool memory(char *s, int *base, expressionS *disp)
{
  char *open = strchr(s, '('), *close = open ? strrchr(s, ')') : NULL;
  if (!open || !close || close[1]) return false;
  *open = 0; *close = 0;
  *base = regno(trim(open + 1));
  if (*base < 0 || *base >= 16) return false;
  char *offset = trim(s);
  if (!*offset)
    {
      disp->X_op = O_constant;
      disp->X_add_number = 0;
      return true;
    }
  return parse_expr(offset, disp);
}

static void emit_symbol(uint32_t template, char *name,
                        bfd_reloc_code_real_type reloc)
{
  expressionS e;
  if (!parse_expr(name, &e)) { bad(_("invalid symbol expression")); return; }
  fixword(template, &e, false, reloc);
}

static int rfn(const char *name)
{
  static const char *const names[] = {
    "add","adc","sub","sbc","mul","mulh","mulhu","div","divu",
    "rem","remu","and","or","xor","not","sll","srl","sra",
    "rol","ror","seq","sne","slt","sge","sltu","sgeu"};
  for (unsigned i = 0; i < ARRAY_SIZE(names); i++)
    if (!strcasecmp(name, names[i])) return i;
  return -1;
}

static int immop(const char *name)
{
  static const char *const names[] = {"addi","subi","andi","ori","xori",
                                      "slli","srli","srai"};
  for (unsigned i = 0; i < ARRAY_SIZE(names); i++)
    if (!strcasecmp(name, names[i])) return i;
  return -1;
}

static int memop(const char *name, bool store)
{
  static const char *const load[] = {"lb","lbu","lh","lhu","lw"};
  static const char *const stores[] = {"sb","sh","sw"};
  const char *const *v = store ? stores : load;
  unsigned n = store ? ARRAY_SIZE(stores) : ARRAY_SIZE(load);
  for (unsigned i = 0; i < n; i++) if (!strcasecmp(name, v[i])) return i;
  return -1;
}

static int branch(const char *name)
{
  static const char *const names[] = {"beq","bne","blt","bge","bltu","bgeu"};
  for (unsigned i = 0; i < ARRAY_SIZE(names); i++) if (!strcasecmp(name, names[i])) return i;
  return -1;
}

void md_begin(void)
{
  bfd_set_arch_mach(stdoutput, TARGET_ARCH, 0);
  opcode_hash_control = str_htab_create();
}

void md_operand(expressionS *op ATTRIBUTE_UNUSED) {}

void md_assemble(char *line)
{
  char *copy = xstrdup(line), *name = strtok(copy, " \t");
  if (!name) { free(copy); return; }
  char *rest = strtok(NULL, "");
  rest = rest ? trim(rest) : (char *)"";
  int n, a, b, c, fn = rfn(name), io = immop(name), br = branch(name);
  char **av = args(rest, &n);

  if (!strcasecmp(name, "ret")) { word(MYEMU2_OP(7) | (14u << 20)); goto done; }
  if (!strcasecmp(name, "call")) {
    if (n != 1) bad(_("call operands"));
    else emit_symbol(MYEMU2_OP(6), av[0], BFD_RELOC_26);
    goto done;
  }
  if (!strcasecmp(name, "li")) {
    if (n != 2 || (a = regno(av[0])) < 0) { bad(_("li operands")); goto done; }
    expressionS e; if (!parse_expr(av[1], &e)) { bad(_("li expression")); goto done; }
    if (e.X_op == O_constant && e.X_add_number >= -2048 && e.X_add_number <= 2047) {
      word(MYEMU2_OP(1)|MYEMU2_RD(a)|MYEMU2_RA(0)|MYEMU2_IMM12(e.X_add_number));
    } else {
      fixword(MYEMU2_OP(8)|MYEMU2_RD(a), &e, false, BFD_RELOC_HI16_S);
      fixword(MYEMU2_OP(1)|MYEMU2_RD(a)|MYEMU2_RA(a)|3u<<12, &e, false, BFD_RELOC_LO16);
    }
    goto done;
  }
  if (fn >= 0) {
    if (n != 3 || (a=regno(av[0]))<0 || (b=regno(av[1]))<0 || (c=regno(av[2]))<0) bad(_("register operands"));
    else word(MYEMU2_OP(0)|MYEMU2_RD(a)|MYEMU2_RA(b)|MYEMU2_RB(c)|MYEMU2_FN(fn));
    goto done;
  }
  if (io >= 0) {
    if (n != 3 || (a=regno(av[0]))<0 || (b=regno(av[1]))<0) bad(_("immediate operands"));
    else {
      expressionS e; bfd_reloc_code_real_type reloc;
      if (!parse_reloc_expr(av[2], &e, &reloc)) bad(_("immediate expression"));
      else if (e.X_op==O_constant) word(MYEMU2_OP(1)|MYEMU2_RD(a)|MYEMU2_RA(b)|((uint32_t)io<<12)|(e.X_add_number&0xfff));
      else fixword(MYEMU2_OP(1)|MYEMU2_RD(a)|MYEMU2_RA(b)|((uint32_t)io<<12), &e, false, reloc == BFD_RELOC_NONE ? BFD_RELOC_LO16 : reloc);
    }
    goto done;
  }
  if ((a=memop(name, false)) >= 0 || (a=memop(name, true)) >= 0) {
    bool store = !strcasecmp(name,"sb") || !strcasecmp(name,"sh") || !strcasecmp(name,"sw");
    if (n != 2 || (b=regno(av[0]))<0) bad(_("memory operands"));
    else { expressionS e; int base; char *m = store ? av[1] : av[1]; if (!memory(m,&base,&e)) bad(_("memory syntax")); else if (e.X_op==O_constant) { if (e.X_add_number < -4096 || e.X_add_number > 4095) bad(_("memory displacement out of range")); else word(MYEMU2_OP(store?3:2)|MYEMU2_RD(b)|MYEMU2_RA(base)|((uint32_t)(a&7)<<13)|(e.X_add_number&0x1fff)); } else fixword(MYEMU2_OP(store?3:2)|MYEMU2_RD(b)|MYEMU2_RA(base)|((uint32_t)(a&7)<<13), &e, false, BFD_RELOC_32); }
    goto done;
  }
  if (br >= 0) {
    if (n != 3 || (a=regno(av[0]))<0 || (b=regno(av[1]))<0) bad(_("branch operands"));
    else { expressionS e; if (!parse_expr(av[2],&e)) bad(_("branch target")); else fixword(MYEMU2_OP(4)|((uint32_t)br<<23)|((uint32_t)a<<18)|((uint32_t)b<<13), &e, false, BFD_RELOC_32_PCREL); }
    goto done;
  }
  if (!strcasecmp(name,"j") || !strcasecmp(name,"jal")) {
    if (n != 1) bad(_("jump operands"));
    else emit_symbol(MYEMU2_OP(!strcasecmp(name,"j")?5:6), av[0], BFD_RELOC_26);
    goto done;
  }
  if (!strcasecmp(name,"jr") || !strcasecmp(name,"jalr")) { if(n!=1||(a=regno(av[0]))<0) bad(_("jump register")); else word(MYEMU2_OP(7)|((!strcasecmp(name,"jalr"))<<25)|((uint32_t)a<<20)); goto done; }
  if (!strcasecmp(name,"lui")) { if(n!=2||(a=regno(av[0]))<0) bad(_("lui operands")); else { expressionS e; bfd_reloc_code_real_type reloc; if(!parse_reloc_expr(av[1],&e,&reloc)) bad(_("lui expression")); else if(e.X_op==O_constant) word(MYEMU2_OP(8)|MYEMU2_RD(a)|(((uint32_t)e.X_add_number&0xfffff)<<1)); else if(reloc == BFD_RELOC_LO16) bad(_("lui requires %hi")); else fixword(MYEMU2_OP(8)|MYEMU2_RD(a),&e,false,BFD_RELOC_HI16_S); } goto done; }
  if (!strcasecmp(name,"mfsr") || !strcasecmp(name,"mtsr")) { if(n!=2) bad(_("system register operands")); else if(!strcasecmp(name,"mfsr")) { if((a=regno(av[0]))<0||(b=sysreg(av[1]))<0) bad(_("system register")); else word(MYEMU2_OP(9)|((uint32_t)0<<22)|((uint32_t)a<<17)|((uint32_t)b<<11)); } else { if((a=sysreg(av[0]))<0||(b=regno(av[1]))<0) bad(_("system register")); else word(MYEMU2_OP(9)|((uint32_t)1<<22)|((uint32_t)b<<17)|((uint32_t)a<<11)); } goto done; }
  if (!strcasecmp(name,"syscall")) { expressionS e; immediate(av[0],&e); if(e.X_op==O_constant) word(MYEMU2_OP(10)|(e.X_add_number&0x3ffffff)); else emit_symbol(MYEMU2_OP(10),av[0],BFD_RELOC_32); goto done; }
  if (!strcasecmp(name,"tlbflush")) { if(n==0) word(MYEMU2_OP(11)); else if(n==1&&(a=regno(av[0]))>=0) word(MYEMU2_OP(11)|(1u<<25)|((uint32_t)a<<20)); else bad(_("tlbflush operands")); goto done; }
  if (!strcasecmp(name,"cas")) { if(n!=3||(a=regno(av[0]))<0||(b=regno(av[1]))<0) bad(_("cas operands")); else { expressionS e; int base; if(!memory(av[2],&base,&e)||e.X_op!=O_constant||e.X_add_number!=0) bad(_("cas address")); else word(MYEMU2_OP(12)|MYEMU2_RD(a)|MYEMU2_RA(b)|((uint32_t)base<<11)); } goto done; }
  if (!strcasecmp(name,"rfe")) word(MYEMU2_OP(9)|(2u<<22));
  else if (!strcasecmp(name,"halt")) word(MYEMU2_OP(9)|(3u<<22));
  else if (!strcasecmp(name,"break")) word(MYEMU2_OP(9)|(4u<<22));
  else bad(_("unknown opcode"));
done:
  dwarf2_emit_insn(4); free(av); free(copy);
}

void md_number_to_chars(char *p, valueT v, int n) { number_to_chars_littleendian(p, v, n); }
const char *md_atof(int type, char *lit, int *size) { *size=0; return _("floating point unsupported"); }
int md_parse_option(int c ATTRIBUTE_UNUSED, const char *a ATTRIBUTE_UNUSED) { return 0; }
void md_show_usage(FILE *s ATTRIBUTE_UNUSED) {}
void
md_apply_fix (fixS *f, valueT *v, segT s)
{
  /* GCC emits .2byte differences in DWARF line tables.  These are
     resolved entirely within the debug section and are not part of the
     REM ELF relocation ABI.  Resolve them here instead of passing the
     generic BFD_RELOC_16 fixup to tc_gen_reloc, where there is deliberately
     no 16-bit REM relocation.  */
  if (f->fx_r_type == BFD_RELOC_16
      && (f->fx_addsy == NULL
          || (f->fx_subsy != NULL
              && S_GET_SEGMENT (f->fx_addsy) == s
              && S_GET_SEGMENT (f->fx_subsy) == s)))
    {
      number_to_chars_littleendian (f->fx_frag->fr_literal + f->fx_where,
                                    *v, 2);
      f->fx_done = 1;
    }
}
arelent *tc_gen_reloc(asection *section ATTRIBUTE_UNUSED, fixS *fixP)
{
  arelent *relP = notes_alloc(sizeof(*relP));
  relP->sym_ptr_ptr = notes_alloc(sizeof(asymbol *));
  *relP->sym_ptr_ptr = symbol_get_bfdsym(fixP->fx_addsy);
  relP->address = fixP->fx_frag->fr_address + fixP->fx_where;
  relP->addend = fixP->fx_offset;
  relP->howto = bfd_reloc_type_lookup(stdoutput, fixP->fx_r_type);
  if (!relP->howto)
    as_fatal(_("unsupported MyEmulator2 relocation type %d"), fixP->fx_r_type);
  return relP;
}

long md_pcrel_from(fixS *fixP)
{
  return fixP->fx_frag->fr_address + fixP->fx_where + 4;
}
