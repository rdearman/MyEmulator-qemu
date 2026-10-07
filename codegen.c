#include "chibicc.h"

/* MyEmulator2 backend.  This first port deliberately implements the integer
 * and pointer subset used by ordinary small C programs.  Unsupported ABI
 * cases fail explicitly instead of being emitted as misleading assembly. */
static FILE *output_file;
static Obj *current_fn;
static int labelseq;
static int eval_depth;

/* Keep the compiler itself within the C subset accepted by the native REM
 * bootstrap.  This is only used to encode an alignment exponent. */
static int rem_ctz(unsigned x) {
  int n = 0;
  if (!x) return 32;
  while ((x & 1) == 0) {
    x >>= 1;
    n++;
  }
  return n;
}

__attribute__((format(printf, 1, 2)))
static void println(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(output_file, fmt, ap);
  va_end(ap);
  fputc('\n', output_file);
}

int align_to(int n, int align) { return (n + align - 1) / align * align; }

static int count(void) { return ++labelseq; }
/* Keep BRANCH13 local; the following J reaches large generated bodies. */
static void branch_zero(int reg, bool nonzero, char *target) {
  int n = count();
  println("  %s r%d, r0, .Lbranch_skip%d", nonzero ? "beq" : "bne", reg, n);
  println("  j %s", target);
  println(".Lbranch_skip%d:", n);
}
static void unsupported(Node *n, char *s) {
  if (n && n->tok) error_tok(n->tok, "REM backend: %s", s);
  error("REM backend: %s", s);
}

/* REM immediate fields are signed 12-bit values.  Large local frames and
 * arrays therefore need an explicit address calculation; allowing GAS to
 * truncate an addi/subi silently turns (for example) a 4160-byte frame into
 * a 64-byte frame and corrupts the compiler at runtime. */
static bool fits_imm12(int n) { return n >= -2048 && n <= 2047; }
static void emit_imm(int reg, int64_t value) {
  if (value >= -2048 && value <= 2047) {
    println("  li r%d, %ld", reg, (long)value);
    return;
  }
  uint32_t u = (uint32_t)value;
  /* LUI supplies bits 31:12 and ORI supplies the low 12 bits. */
  println("  lui r%d, %u", reg, (unsigned)(u >> 12));
  println("  ori r%d, r%d, %u", reg, reg, (unsigned)(u & 0xfff));
}
static void add_offset(int dst, int base, int off, int tmp) {
  if (fits_imm12(off)) {
    println("  addi r%d, r%d, %d", dst, base, off);
    return;
  }
  emit_imm(tmp, off);
  println("  add r%d, r%d, r%d", dst, base, tmp);
}
static void load_fp(int dst, int off) {
  if (fits_imm12(off)) println("  lw r%d, %d(r5)", dst, off);
  else { emit_imm(12, off); println("  add r12, r5, r12"); println("  lw r%d, 0(r12)", dst); }
}
static void store_fp(int src, int off) {
  if (fits_imm12(off)) println("  sw r%d, %d(r5)", src, off);
  else {
    emit_imm(12, off);
    println("  add r12, r5, r12");
    println("  sw r%d, 0(r12)", src);
  }
}

static void push_reg(int r) {
  println("  subi r13, r13, 4");
  println("  sw r%d, 0(r13)", r);
  eval_depth++;
}
static void pop_reg(int r) {
  println("  lw r%d, 0(r13)", r);
  println("  addi r13, r13, 4");
  eval_depth--;
}

/* Arrays decay to one pointer-sized argument at a call boundary.  The
 * expression still has its array type here (notably for string literals), so
 * using ty->size > 4 directly would incorrectly pass a string as two words
 * and shift every following argument. */
static int arg_words(Type *ty) {
  if (!ty || ty->kind == TY_ARRAY || ty->kind == TY_FUNC)
    return 1;
  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION)
    return 1;
  return ty->size > 4 ? 2 : 1;
}

static void load(Type *ty) {
  if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT || ty->kind == TY_UNION ||
      ty->kind == TY_FUNC || ty->kind == TY_VLA)
    return;
  if (ty->kind == TY_DOUBLE || ty->kind == TY_LDOUBLE) {
    println("  lw r2, 4(r1)");
    println("  lw r1, 0(r1)");
    return;
  }
  if (ty->kind == TY_FLOAT)
    { println("  lw r1, 0(r1)"); return; }
  if (ty->size == 8) {
    println("  lw r2, 4(r1)");
    println("  lw r1, 0(r1)");
    return;
  }
  switch (ty->size) {
  case 1: println("  %s r1, 0(r1)", ty->is_unsigned ? "lbu" : "lb"); return;
  case 2: println("  %s r1, 0(r1)", ty->is_unsigned ? "lhu" : "lh"); return;
  case 4: println("  lw r1, 0(r1)"); return;
  default: unsupported(NULL, "loads wider than 32 bits are not implemented yet");
  }
}

static void store(Type *ty) {
  /* A 64-bit value occupies r1:r2.  Preserve both value words while
   * recovering the destination address in r3; using r2 here aliases the
   * high value word and leaves the later 64-bit stores targeting whatever
   * stale value happens to be in r3. */
  bool aggregate = ty->kind == TY_STRUCT || ty->kind == TY_UNION ||
                   ty->kind == TY_ARRAY;
  int addr_reg = !aggregate && (ty->kind == TY_DOUBLE || ty->kind == TY_LDOUBLE ||
                  ty->size == 8) ? 3 : 2;
  pop_reg(addr_reg);                 /* address */
  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION || ty->kind == TY_ARRAY) {
    int n = count();
    println("  add r6, r2, r0");
    println("  add r3, r1, r0");
    emit_imm(4, ty->size);
    println(".Lcopy%d:", n);
    println("  beq r4, r0, .Lcopy_end%d", n);
    println("  lbu r1, 0(r3)");
    println("  sb r1, 0(r2)");
    println("  addi r3, r3, 1");
    println("  addi r2, r2, 1");
    println("  subi r4, r4, 1");
    println("  j .Lcopy%d", n);
    println(".Lcopy_end%d:", n);
    println("  add r1, r6, r0");
    return;
  }
  if (ty->kind == TY_DOUBLE || ty->kind == TY_LDOUBLE) {
    println("  sw r1, 0(r3)");
    println("  sw r2, 4(r3)");
    return;
  }
  if (ty->kind == TY_FLOAT) { println("  sw r1, 0(r2)"); return; }
  if (ty->size == 8) {
    println("  sw r1, 0(r3)");
    println("  sw r2, 4(r3)");
    return;
  }
  switch (ty->size) {
  case 1: println("  sb r1, 0(r2)"); return;
  case 2: println("  sh r1, 0(r2)"); return;
  case 4: println("  sw r1, 0(r2)"); return;
  default: unsupported(NULL, "stores wider than 32 bits are not implemented yet");
  }
}

static void extract_bitfield(Member *mem) {
  if (mem->ty->size > 4 || mem->bit_width + mem->bit_offset > 32)
    unsupported(NULL, "64-bit bitfield storage is not implemented yet");
  println("  slli r1, r1, %d", 32 - mem->bit_width - mem->bit_offset);
  println("  %s r1, r1, %d",
          mem->ty->is_unsigned || mem->ty->kind == TY_BOOL ? "srli" : "srai",
          32 - mem->bit_width);
}

static void store_bitfield(Member *mem) {
  if (mem->ty->size > 4 || mem->bit_width + mem->bit_offset > 32)
    unsupported(NULL, "64-bit bitfield storage is not implemented yet");
  uint32_t mask = mem->bit_width == 32 ? UINT32_MAX :
                  (1U << mem->bit_width) - 1;
  pop_reg(2);
  emit_imm(3, mask);
  println("  and r1, r1, r3");
  println("  slli r1, r1, %d", mem->bit_offset);
  emit_imm(4, ~(mask << mem->bit_offset));
  println("  %s r3, 0(r2)",
          mem->ty->size == 1 ? "lbu" : mem->ty->size == 2 ? "lhu" : "lw");
  println("  and r3, r3, r4");
  println("  or r3, r3, r1");
  println("  %s r3, 0(r2)",
          mem->ty->size == 1 ? "sb" : mem->ty->size == 2 ? "sh" : "sw");
  println("  add r1, r3, r0");
  extract_bitfield(mem);
}

static void gen_expr(Node *node);
static void gen_stmt(Node *node);

static void push_pair(void) {
  println("  subi r13, r13, 4");
  println("  sw r2, 0(r13)");
  println("  subi r13, r13, 4");
  println("  sw r1, 0(r13)");
  eval_depth += 2;
}

static void pop_pair(int lo, int hi) {
  pop_reg(lo);
  pop_reg(hi);
}

/* libgcc's 64-bit helpers are ordinary hosted ABI callees.  They may save
 * incoming argument registers in the caller-provided home area, just like a
 * libc function.  The stage-1 compiler happened to survive without this
 * reservation because its own generated callers did not exercise the helper
 * path; stage-2's 64-bit hashmap code exposed the missing area as a SIGBUS.
 */
static void call_wide_helper(char *name) {
  /* Values left on the expression stack (notably an assignment
   * destination) can leave SP four or eight bytes off the ABI boundary.
   * Align before reserving the helper's register home area. */
  int pad = (16 - (eval_depth * 4) % 16) & 15;
  if (pad)
    println("  subi r13, r13, %d", pad);
  println("  subi r13, r13, 32");
  println("  jal %s", name);
  println("  addi r13, r13, %d", 32 + pad);
}

static void gen_wide_binary(Node *node) {
  gen_expr(node->lhs);
  push_pair();
  gen_expr(node->rhs);
  pop_pair(3, 4);             /* lhs low/high; rhs remains r1:r2 */
  switch (node->kind) {
  case ND_ADD:
    println("  add r1, r3, r1");
    println("  sltu r3, r1, r3");
    println("  add r2, r4, r2");
    println("  add r2, r2, r3");
    return;
  case ND_SUB:
    println("  sltu r6, r3, r1");
    println("  sub r1, r3, r1");
    println("  sub r2, r4, r2");
    println("  sub r2, r2, r6");
    return;
  case ND_BITAND: println("  and r1, r3, r1"); println("  and r2, r4, r2"); return;
  case ND_BITOR:  println("  or r1, r3, r1");  println("  or r2, r4, r2");  return;
  case ND_BITXOR: println("  xor r1, r3, r1"); println("  xor r2, r4, r2"); return;
  case ND_EQ:
    println("  seq r1, r3, r1"); println("  seq r2, r4, r2");
    println("  and r1, r1, r2"); println("  li r2, 0"); return;
  case ND_NE:
    println("  sne r1, r3, r1"); println("  sne r2, r4, r2");
    println("  or r1, r1, r2"); println("  li r2, 0"); return;
  case ND_LT:
  case ND_LE: {
    /* Compare the high word first.  A low-word comparison is relevant only
     * when the high words are equal; ORing the two comparisons directly
     * makes (high_lhs > high_rhs, low_lhs < low_rhs) incorrectly true. */
    const char *cmp = node->lhs->ty->is_unsigned ? "sltu" : "slt";
    println("  %s r6, r4, r2", cmp);       /* high_lhs < high_rhs */
    println("  seq r7, r4, r2");           /* high words equal */
    println("  sltu r8, r3, r1");
    println("  seq r9, r3, r1");
    println("  and r8, r8, r7");
    println("  or r1, r6, r8");
    if (node->kind == ND_LE) {
      println("  and r9, r9, r7");
      println("  or r1, r1, r9");
    }
    println("  li r2, 0");
    return;
  }
  case ND_MUL:
    println("  add r6, r1, r0"); println("  add r7, r2, r0");
    println("  add r1, r3, r0"); println("  add r2, r4, r0");
    println("  add r3, r6, r0"); println("  add r4, r7, r0");
    call_wide_helper("__muldi3");
    return;
  case ND_DIV:
  case ND_MOD:
    println("  add r6, r1, r0"); println("  add r7, r2, r0");
    println("  add r1, r3, r0"); println("  add r2, r4, r0");
    println("  add r3, r6, r0"); println("  add r4, r7, r0");
    if (node->ty->is_unsigned)
      call_wide_helper(node->kind == ND_DIV ? "__udivdi3" : "__umoddi3");
    else
      call_wide_helper(node->kind == ND_DIV ? "__divdi3" : "__moddi3");
    return;
  case ND_SHL:
    println("  add r6, r1, r0");
    println("  add r1, r3, r0"); println("  add r2, r4, r0");
    println("  add r3, r6, r0");
    call_wide_helper("__ashldi3"); return;
  case ND_SHR:
    println("  add r6, r1, r0");
    println("  add r1, r3, r0"); println("  add r2, r4, r0");
    println("  add r3, r6, r0");
    call_wide_helper(node->lhs->ty->is_unsigned ? "__lshrdi3" : "__ashrdi3");
    return;
  default:
    unsupported(node, "64-bit operator is not implemented yet");
  }
}

static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local) {
      if (node->var->is_by_ref || node->var->ty->kind == TY_VLA) {
        load_fp(1, node->var->offset);
        return;
      }
      add_offset(1, 5, node->var->offset, 2);
      return;
    }
    println("  lui r1, %%hi(%s)", node->var->name);
    println("  ori r1, r1, %%lo(%s)", node->var->name);
    return;
  case ND_DEREF:
    gen_expr(node->lhs); return;
  case ND_COMMA:
    gen_expr(node->lhs); gen_addr(node->rhs); return;
  case ND_MEMBER:
    gen_addr(node->lhs);
    if (node->member->offset)
      add_offset(1, 1, node->member->offset, 2);
    return;
  case ND_VLA_PTR:
    add_offset(1, 5, node->var->offset, 2); return;
  case ND_FUNCALL:
  case ND_ASSIGN:
  case ND_COND:
  case ND_STMT_EXPR:
    if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION) {
      gen_expr(node);
      return;
    }
    break;
  default: break;
  }
  unsupported(node, "expression is not an lvalue");
}

static void cmp_zero(Type *ty) {
  if (is_flonum(ty)) {
    emit_imm(3, 0x7fffffff);
    if (ty->size == 8) {
      println("  and r2, r2, r3");
      println("  or r1, r1, r2");
    } else
      println("  and r1, r1, r3");
  } else if (ty->size == 8)
    println("  or r1, r1, r2");
  println("  sne r1, r1, r0");
}

static void gen_float_binary(Node *node) {
  bool wide = node->lhs->ty->size == 8;
  gen_expr(node->lhs);
  if (wide) push_pair(); else push_reg(1);
  gen_expr(node->rhs);
  if (wide) {
    println("  add r3, r1, r0");
    println("  add r4, r2, r0");
    pop_pair(1, 2);
  } else {
    println("  add r2, r1, r0");
    pop_reg(1);
  }
  char *op;
  switch (node->kind) {
  case ND_ADD: op = "add"; break;
  case ND_SUB: op = "sub"; break;
  case ND_MUL: op = "mul"; break;
  case ND_DIV: op = "div"; break;
  case ND_EQ: case ND_NE: op = "eq"; break;
  case ND_LT: op = "lt"; break;
  case ND_LE: op = "le"; break;
  default: unsupported(node, "invalid floating-point operator");
  }
  char *helper = format("__%s%s%s", op, wide ? "df" : "sf",
                       node->kind == ND_EQ || node->kind == ND_NE ||
                       node->kind == ND_LT || node->kind == ND_LE ? "2" : "3");
  call_wide_helper(helper);
  free(helper);
  switch (node->kind) {
  case ND_EQ: println("  seq r1, r1, r0"); break;
  case ND_NE: println("  sne r1, r1, r0"); break;
  case ND_LT: println("  slt r1, r1, r0"); break;
  case ND_LE: println("  slt r1, r0, r1"); println("  xori r1, r1, 1"); break;
  default: break;
  }
}

static void gen_cast(Type *from, Type *to) {
  if (to->kind == TY_VOID) return;
  if (to->kind == TY_BOOL) { cmp_zero(from); return; }
  if (is_flonum(from) && is_flonum(to)) {
    if (from->size != to->size)
      call_wide_helper(to->size == 8 ? "__extendsfdf2" : "__truncdfsf2");
    return;
  }
  if (is_flonum(to)) {
    char *helper = format("__float%s%s%s",
                          from->is_unsigned ? "un" : "",
                          from->size == 8 ? "di" : "si",
                          to->size == 8 ? "df" : "sf");
    call_wide_helper(helper);
    free(helper);
    return;
  }
  if (is_flonum(from)) {
    char *helper = format("__fix%s%s%s",
                          to->is_unsigned ? "uns" : "",
                          from->size == 8 ? "df" : "sf",
                          to->size == 8 ? "di" : "si");
    call_wide_helper(helper);
    free(helper);
  } else if (to->size == 8 && from->size < 8)
    println(from->is_unsigned ? "  li r2, 0" : "  srai r2, r1, 31");
  if (to->size < 4) {
    int shift = 32 - to->size * 8;
    println("  slli r1, r1, %d", shift);
    println("  %s r1, r1, %d", to->is_unsigned ? "srli" : "srai", shift);
  }
}

static void gen_call(Node *node) {
  if (node->lhs->kind == ND_VAR &&
      !strcmp(node->lhs->var->name, "alloca")) {
    gen_expr(node->args);
    println("  addi r1, r1, 15");
    emit_imm(2, -16);
    println("  and r1, r1, r2");
    println("  add r3, r13, r0");
    println("  sub r13, r13, r1");
    if (eval_depth) {
      int label = count();
      println("  add r2, r13, r0");
      emit_imm(4, eval_depth * 4);
      println(".Lalloca_copy%d:", label);
      println("  beq r4, r0, .Lalloca_end%d", label);
      println("  lbu r1, 0(r3)");
      println("  sb r1, 0(r2)");
      println("  addi r3, r3, 1");
      println("  addi r2, r2, 1");
      println("  subi r4, r4, 1");
      println("  j .Lalloca_copy%d", label);
      println(".Lalloca_end%d:", label);
    }
    add_offset(1, 13, eval_depth * 4, 12);
    return;
  }
  int nargs = 0;
  for (Node *a = node->args; a; a = a->next) nargs++;
  bool indirect = !(node->lhs->kind == ND_VAR && node->lhs->var->is_function);
  int nwords = 0;
  bool aggregate_return = node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION;
  bool hidden_return = aggregate_return && node->ty->size > 8;
  if (hidden_return) nwords++;
  for (Node *a = node->args; a; a = a->next)
    nwords += arg_words(a->ty);
  /* The REM ABI requires SP to be 16-byte aligned at every call boundary.
     Register arguments are popped before the call, so only arguments which
     remain on the outgoing stack (plus the ABI's 32-byte home area) affect
     the boundary alignment.  Padding the complete argument list here is
     wrong for ordinary one- and two-argument calls: it leaves that padding
     below an otherwise register-only call and passes a misaligned SP to the
     callee. */
  int regwords = 0;
  regwords = nwords < 4 ? nwords : 4;
  int stackwords = nwords - regwords;
  /* An indirect callee address is kept below outgoing stack arguments until
     after the call.  Include that temporary word in alignment when needed. */
  int tempwords = indirect && stackwords ? 1 : 0;
  int call_pad = (16 - ((eval_depth + stackwords + tempwords) * 4) % 16) & 15;
  if (call_pad) {
    println("  subi r13, r13, %d", call_pad);
    eval_depth += call_pad / 4;
  }
  Node **args = calloc(nargs, sizeof(Node *));
  if (indirect) {
    gen_expr(node->lhs);
    push_reg(1);
  }
  int ai = 0;
  for (Node *a = node->args; a; a = a->next) args[ai++] = a;
  /* Evaluate right-to-left so that the first four values can be popped into
     registers while additional values remain in call order on the stack. */
  for (int i = nargs - 1; i >= 0; i--) {
    gen_expr(args[i]);
    if (arg_words(args[i]->ty) == 2)
      push_pair();
    else
      push_reg(1);
  }
  if (hidden_return) {
    add_offset(1, 5, node->ret_buffer->offset, 12);
    push_reg(1);
  }
  int argreg = 1;
  if (hidden_return) pop_reg(argreg++);
  for (int i = 0; i < nargs; i++) {
    for (int j = 0; j < arg_words(args[i]->ty) && argreg <= 4; j++)
      pop_reg(argreg++);
  }
  int extra = stackwords;
  if (indirect) {
    if (!extra) {
      pop_reg(15);
    } else {
      /* Stack arguments are above the saved callee address. */
      println("  lw r15, %d(r13)", extra * 4);
    }
  }
  /* REM's hosted ABI gives every callee an outgoing register home area.
     The musl/GCC prologue saves incoming r1-r4 at old-SP+16..28 even for
     fixed-arity functions.  Omitting this area lets a callee overwrite its
     caller's saved frame/link words; the corruption is especially visible
     when a fixed-arity libc call is made from a helper function. */
  bool reserve_home = true;
  if (reserve_home)
    println("  subi r13, r13, 32");
  if (!indirect)
    println("  jal %s", node->lhs->var->name);
  else
    println("  jalr r15");
  if (reserve_home) {
    println("  addi r13, r13, %d", 32 + extra * 4 + call_pad);
    eval_depth -= extra;
    if (indirect && extra) {
      println("  addi r13, r13, 4");
      eval_depth--;
    }
  } else if (call_pad)
    println("  addi r13, r13, %d", call_pad);
  eval_depth -= call_pad / 4;
  if (aggregate_return) {
    add_offset(3, 5, node->ret_buffer->offset, 12);
    if (!hidden_return) {
      for (int i = 0; i < node->ty->size; i++) {
        println("  srli r4, r%d, %d", i < 4 ? 1 : 2, (i % 4) * 8);
        println("  sb r4, %d(r3)", i);
      }
    }
    println("  add r1, r3, r0");
  }
  free(args);
}

static void gen_expr(Node *node) {
  if (!node) return;
  switch (node->kind) {
  case ND_NULL_EXPR: return;
  case ND_MEMZERO: {
    int label = count();
    gen_addr((Node *)&(Node){.kind = ND_VAR, .var = node->var});
    emit_imm(3, node->var->ty->size);
    println(".Lzero%d:", label);
    println("  beq r3, r0, .Lzero_end%d", label);
    println("  sb r0, 0(r1)");
    println("  addi r1, r1, 1");
    println("  subi r3, r3, 1");
    println("  j .Lzero%d", label);
    println(".Lzero_end%d:", label);
    return;
  }
  case ND_NUM:
    if (node->ty->kind == TY_FLOAT || node->ty->kind == TY_DOUBLE || node->ty->kind == TY_LDOUBLE) {
      /* The initial REM port only needs the zero constant for Samurai's
         optional load-average path.  Keep the representation ABI-correct:
         doubles are returned as r1:r2. */
      if (node->ty->size == 4) {
        uint32_t bits;
        rem_float_bits(&node->fval, &bits);
        emit_imm(1, bits);
      } else {
        uint64_t bits;
        memcpy(&bits, &node->fval, sizeof(bits));
        emit_imm(1, (uint32_t)bits);
        emit_imm(2, (uint32_t)(bits >> 32));
      }
      return;
    }
    /* Integer literals used in 64-bit expressions can arrive here with a
       narrower apparent type while still carrying a full-width value (for
       example the FNV constants in hashmap.c).  Do not print such values
       through the 32-bit immediate form: GAS quite correctly rejects the
       resulting out-of-range literal, and a bootstrap compiler would emit
       corrupt code.  Materialise both words whenever the value is outside
       the signed 32-bit range. */
    if (node->ty->size == 8 || node->val > INT32_MAX || node->val < INT32_MIN) {
      uint64_t v = (uint64_t)node->val;
      emit_imm(1, (uint32_t)v);
      emit_imm(2, (uint32_t)(v >> 32));
    } else {
      println("  li r1, %ld", (long)node->val);
    }
    return;
  case ND_VAR:
    gen_addr(node); load(node->ty); return;
  case ND_MEMBER:
    gen_addr(node);
    load(node->ty);
    if (node->member->is_bitfield)
      extract_bitfield(node->member);
    return;
  case ND_DEREF:
    gen_expr(node->lhs); load(node->ty); return;
  case ND_ADDR: gen_addr(node->lhs); return;
  case ND_ASSIGN:
    gen_addr(node->lhs);
    push_reg(1);
    gen_expr(node->rhs);
    if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield)
      store_bitfield(node->lhs->member);
    else
      store(node->ty);
    return;
  case ND_COMMA: gen_expr(node->lhs); gen_expr(node->rhs); return;
  case ND_CAST:
    gen_expr(node->lhs);
    gen_cast(node->lhs->ty, node->ty);
    return;
  case ND_NEG:
    gen_expr(node->lhs);
    if (is_flonum(node->ty)) {
      emit_imm(3, 0x80000000);
      println("  xor r%d, r%d, r3", node->ty->size == 8 ? 2 : 1,
              node->ty->size == 8 ? 2 : 1);
    } else if (node->ty->size == 8) {
      println("  sub r1, r0, r1");
      println("  not r2, r2, r0");
      println("  seq r3, r1, r0");
      println("  add r2, r2, r3");
    } else println("  sub r1, r0, r1");
    return;
  case ND_BITNOT:
    gen_expr(node->lhs);
    if (node->ty->size == 8) { println("  not r1, r1, r0"); println("  not r2, r2, r0"); }
    else println("  not r1, r1, r0");
    return;
  case ND_NOT: gen_expr(node->lhs); cmp_zero(node->lhs->ty); println("  xori r1, r1, 1"); return;
  case ND_LOGAND: {
    int n = count(); gen_expr(node->lhs); cmp_zero(node->lhs->ty); branch_zero(1, false, format(".Lfalse%d", n));
    gen_expr(node->rhs); cmp_zero(node->rhs->ty); branch_zero(1, false, format(".Lfalse%d", n)); println("  li r1, 1");
    println("  j .Lend%d", n); println(".Lfalse%d:", n); println("  li r1, 0"); println(".Lend%d:", n); return;
  }
  case ND_LOGOR: {
    int n = count(); gen_expr(node->lhs); cmp_zero(node->lhs->ty); branch_zero(1, true, format(".Ltrue%d", n));
    gen_expr(node->rhs); cmp_zero(node->rhs->ty); branch_zero(1, true, format(".Ltrue%d", n)); println("  li r1, 0");
    println("  j .Lend%d", n); println(".Ltrue%d:", n); println("  li r1, 1"); println(".Lend%d:", n); return;
  }
  case ND_COND: {
    int n = count(); gen_expr(node->cond); cmp_zero(node->cond->ty); branch_zero(1, false, format(".Lelse%d", n));
    gen_expr(node->then); println("  j .Lend%d", n); println(".Lelse%d:", n); gen_expr(node->els); println(".Lend%d:", n); return;
  }
  case ND_FUNCALL: gen_call(node); return;
  case ND_VA_START: {
    if (!node->rhs || node->rhs->kind != ND_VAR)
      unsupported(node, "va_start requires a named parameter");
    gen_addr(node->lhs);
    push_reg(1);
    /* Variadic entry points keep a canonical incoming-register save area at
       16..28.  Named parameters are homed separately (after that area), so
       the va_list cursor must be derived from the parameter's argument
       position rather than its local home offset. */
    int arg_index = 0;
    for (Obj *v = current_fn->params; v && v != node->rhs->var; v = v->next)
      arg_index += v->ty->size > 4 ? 2 : 1;
    /* The register save area starts at old-SP+16, which is above this
       function's frame. */
    arg_index += arg_words(node->rhs->var->ty);
    load_fp(1, 28);
    add_offset(1, 1, 16 + arg_index * 4, 2);
    store(pointer_to(ty_char));
    return;
  }
  case ND_VA_ARG: {
    gen_addr(node->lhs);
    println("  add r4, r1, r0");
    println("  lw r3, 0(r4)");
    switch (node->va_arg_ty->size) {
    case 4: println("  lw r1, 0(r3)"); break;
    case 8: println("  lw r1, 0(r3)"); println("  lw r2, 4(r3)"); break;
    default: unsupported(node, "va_arg requires a promoted scalar type");
    }
    println("  addi r3, r3, %d", node->va_arg_ty->size);
    println("  sw r3, 0(r4)");
    return;
  }
  case ND_VA_END: return;
  case ND_VA_COPY:
    gen_expr(node->rhs);
    push_reg(1);
    gen_addr(node->lhs);
    pop_reg(2);
    println("  sw r2, 0(r1)");
    return;
  default: break;
  }

  if (node->lhs && node->lhs->ty && is_flonum(node->lhs->ty)) {
    gen_float_binary(node);
    return;
  }
  if (node->lhs && node->lhs->ty && node->lhs->ty->size == 8) {
    gen_wide_binary(node);
    return;
  }

  gen_expr(node->lhs); push_reg(1); gen_expr(node->rhs); pop_reg(2);
  switch (node->kind) {
  case ND_ADD: println("  add r1, r2, r1"); return;
  case ND_SUB: println("  sub r1, r2, r1"); return;
  case ND_MUL: println("  mul r1, r2, r1"); return;
  case ND_DIV: println("  %s r1, r2, r1", node->ty->is_unsigned ? "divu" : "div"); return;
  case ND_MOD: println("  %s r1, r2, r1", node->ty->is_unsigned ? "remu" : "rem"); return;
  case ND_BITAND: println("  and r1, r2, r1"); return;
  case ND_BITOR: println("  or r1, r2, r1"); return;
  case ND_BITXOR: println("  xor r1, r2, r1"); return;
  case ND_SHL: println("  sll r1, r2, r1"); return;
  case ND_SHR: println("  %s r1, r2, r1", node->lhs->ty->is_unsigned ? "srl" : "sra"); return;
  case ND_EQ: println("  seq r1, r2, r1"); return;
  case ND_NE: println("  sne r1, r2, r1"); return;
  case ND_LT: println("  %s r1, r2, r1", node->lhs->ty->is_unsigned ? "sltu" : "slt"); return;
  case ND_LE:
    println("  %s r1, r1, r2", node->lhs->ty->is_unsigned ? "sltu" : "slt");
    println("  xori r1, r1, 1"); return;
  default: unsupported(node, "expression operator is not implemented yet");
  }
}

// Emit a DWARF line-table entry (.loc) for the source line of a token.
static void emit_loc(Token *tok) {
  static int last_file = -1;
  static int last_line = -1;
  if (!opt_g || !tok || !tok->file) return;
  if (tok->file->file_no == last_file && tok->line_no == last_line) return;
  last_file = tok->file->file_no;
  last_line = tok->line_no;
  println("  .loc %d %d", last_file, last_line);
}

static void gen_stmt(Node *node) {
  if (!node) return;
  if (node->kind != ND_BLOCK) emit_loc(node->tok);
  switch (node->kind) {
  case ND_IF: {
    int n = count(); gen_expr(node->cond); cmp_zero(node->cond->ty); branch_zero(1, false, format(".Lelse%d", n));
    gen_stmt(node->then); println("  j .Lend%d", n); println(".Lelse%d:", n);
    if (node->els) gen_stmt(node->els);
    println(".Lend%d:", n);
    return;
  }
  case ND_FOR: {
    if (node->init) gen_stmt(node->init);
    println("%s:", node->begin_label);
    if (node->cond) { gen_expr(node->cond); cmp_zero(node->cond->ty); branch_zero(1, false, node->brk_label); }
    gen_stmt(node->then);
    if (node->cont_label) println("%s:", node->cont_label);
    if (node->inc) {
      emit_loc(node->inc->tok);
      gen_expr(node->inc);
    }
    println("  j %s", node->begin_label);
    println("%s:", node->brk_label); return;
  }
  case ND_DO: {
    println("%s:", node->begin_label); gen_stmt(node->then);
    if (node->cont_label) println("%s:", node->cont_label);
    emit_loc(node->cond->tok);
    gen_expr(node->cond);
    cmp_zero(node->cond->ty);
    branch_zero(1, true, node->begin_label); println("%s:", node->brk_label); return;
  }
  case ND_BLOCK: for (Node *n = node->body; n; n = n->next) gen_stmt(n); return;
  case ND_RETURN:
    if (node->lhs) gen_expr(node->lhs);
    else println("  li r1, 0");
    if (current_fn->ty->return_ty->kind == TY_STRUCT ||
        current_fn->ty->return_ty->kind == TY_UNION) {
      Type *ty = current_fn->ty->return_ty;
      if (ty->size > 8) {
        println("  add r3, r1, r0");
        load_fp(1, current_fn->params->offset);
        push_reg(1);
        println("  add r1, r3, r0");
        store(ty);
      } else {
        println("  add r3, r1, r0");
        println("  li r1, 0");
        println("  li r2, 0");
        for (int i = 0; i < ty->size; i++) {
          println("  lbu r4, %d(r3)", i);
          println("  slli r4, r4, %d", (i % 4) * 8);
          println("  or r%d, r%d, r4", i < 4 ? 1 : 2, i < 4 ? 1 : 2);
        }
      }
      println("  j .Lreturn.%s", current_fn->name);
      return;
    }
    /* A 32-bit expression returned from a 64-bit function must still
       populate the high return word.  Without this, `return -1` leaves r2
       stale and callers comparing an int64_t sentinel observe garbage. */
    if (current_fn->ty->return_ty->size == 8 &&
        (!node->lhs || node->lhs->ty->size < 8)) {
      if (node->lhs && node->lhs->ty->is_unsigned)
        println("  li r2, 0");
      else
        println("  srai r2, r1, 31");
    }
    println("  j .Lreturn.%s", current_fn->name); return;
  case ND_EXPR_STMT: gen_expr(node->lhs); return;
  case ND_NULL_EXPR: return;
  case ND_GOTO: println("  j %s", node->unique_label); return;
  case ND_LABEL: println("%s:", node->unique_label); gen_stmt(node->lhs); return;
  case ND_CASE: println("%s:", node->label); gen_stmt(node->lhs); return;
  case ND_SWITCH: {
    gen_expr(node->cond);
    println("  add r15, r1, r0");
    for (Node *c = node->case_next; c; c = c->case_next) {
      println("  li r1, %ld", c->begin);
      println("  seq r1, r15, r1");
      branch_zero(1, true, c->label);
    }
    if (node->default_case)
      println("  j %s", node->default_case->label);
    else
      println("  j %s", node->brk_label);
    gen_stmt(node->then);
    println("%s:", node->brk_label);
    return;
  }
  case ND_ASM: println("  %s", node->asm_str); return;
  default: unsupported(node, "statement is not implemented yet");
  }
}

static void assign_lvar_offsets(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function) continue;
    /* Variadic functions reserve 16..28 for the canonical incoming r1-r4
       save area.  Their named parameter homes therefore begin at 32. */
    /* Each call reserves a 32-byte register home area below the caller's
     * frame.  The callee writes incoming r1-r4 at caller-SP+16..28, so a
     * caller's own locals and parameter homes must begin above that area.
     * Keeping the first user slot at +48 prevents a four-argument call from
     * overwriting the caller's locals (stage-3 chibicc exposed this through
     * fwrite during preprocessing). */
    int off = 48;
    if (fn->va_area)
      off = 64;
    int nparams = 0;
    for (Obj *v = fn->params; v; v = v->next) {
      if (v->ty->kind == TY_STRUCT || v->ty->kind == TY_UNION) {
        v->is_by_ref = true;
        nparams++;
        continue;
      }
      if ((v->ty->kind == TY_STRUCT || v->ty->kind == TY_UNION || v->ty->size > 4) &&
          v->ty->kind != TY_DOUBLE && v->ty->kind != TY_LDOUBLE &&
          v->ty->kind != TY_LLONG) {
        fprintf(stderr, "REM unsupported param %s kind=%d size=%d in %s\n",
                v->name, v->ty->kind, v->ty->size, fn->name);
        unsupported(NULL, "aggregate or 64-bit parameters are not implemented yet");
      }
      nparams++;
    }
    Obj **params = calloc(nparams, sizeof(Obj *));
    int pi = 0;
    for (Obj *v = fn->params; v; v = v->next) params[pi++] = v;
    /* parse.c stores parameter locals in reverse declaration order. */
    for (int i = nparams - 1; i >= 0; i--) {
      params[i]->offset = off;
      off += !params[i]->is_by_ref && params[i]->ty->size > 4 ? 8 : 4;
    }
    free(params);
    /* Locals follow the parameter homes. */
    for (Obj *v = fn->locals; v; v = v->next) {
      if (v->offset) continue;
      if (v->ty->kind != TY_VLA &&
          (v->ty->kind != TY_ARRAY && v->ty->kind != TY_STRUCT &&
           v->ty->kind != TY_UNION && v->ty->kind != TY_DOUBLE &&
           v->ty->kind != TY_LDOUBLE && v->ty->kind != TY_LLONG &&
           v->ty->size > 4)) {
        fprintf(stderr, "REM unsupported local %s kind=%d size=%d in %s\n",
                v->name, v->ty->kind, v->ty->size, fn->name);
        unsupported(NULL, "local type is not implemented yet");
      }
      off = align_to(off, v->align > v->ty->align ? v->align : v->ty->align);
      v->offset = off;
      off += v->ty->kind == TY_VLA ? 4 : v->ty->size;
    }
    fn->stack_size = align_to(off + 16, 16);
  }
}

static void emit_data(Obj *prog) {
  for (Obj *v = prog; v; v = v->next) {
    if (v->is_function || !v->is_definition || v->alias_name) continue;
    println("  %s %s", v->is_static ? ".local" : v->is_weak ? ".weak" : ".global", v->name);
    println("  .type %s, @object", v->name);
    println("  .align %d", v->align > 0 ? rem_ctz(v->align) : 0);
    println("%s:", v->name);
    if (!v->init_data) { println("  .zero %d", v->ty->size); continue; }
    /* Global pointer initializers are recorded as relocations by the
       parser.  Emitting only init_data leaves those words as zero, which
       breaks tables such as Samurai's keyword names. */
    Relocation *rel = v->rel;
    int pos = 0;
    while (pos < v->ty->size) {
      if (rel && rel->offset == pos) {
        if (rel->addend)
          println("  .word %s%+ld", *rel->label, rel->addend);
        else
          println("  .word %s", *rel->label);
        rel = rel->next;
        pos += 4;
      } else {
        println("  .byte %d", (unsigned char)v->init_data[pos++]);
      }
    }
  }
}

static void emit_text(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    /* Emit every defined function.  Function pointers and callback tables
       (used heavily by Samurai) are not always visible to the lightweight
       reference liveness pass. */
    if (!fn->is_function || !fn->is_definition || fn->alias_name) continue;
    println("  %s %s", fn->is_static ? ".local" : fn->is_weak ? ".weak" : ".global", fn->name);
    println("  .text");
    println("  .align 2");
    println("  .type %s, @function", fn->name); println("%s:", fn->name);
    emit_loc(fn->tok);
    current_fn = fn;
    int alignment = 16;
    for (Obj *v = fn->locals; v; v = v->next) {
      if (v->align > alignment) alignment = v->align;
      if (v->ty->align > alignment) alignment = v->ty->align;
    }
    int allocation = fn->stack_size + alignment;
    if (fits_imm12(allocation)) println("  subi r13, r13, %d", allocation);
    else { emit_imm(12, allocation); println("  sub r13, r13, r12"); }
    /* Legacy libc callers can enter with only word alignment. Keep the
       original SP for arguments/return while realigning the active frame. */
    add_offset(12, 13, allocation, 12);
    println("  sw r12, 0(r13)");
    emit_imm(12, alignment - 1);
    println("  and r12, r13, r12");
    println("  sub r13, r13, r12");
    println("  add r12, r13, r12");
    println("  lw r12, 0(r12)");
    println("  sw r12, 28(r13)");
    println("  sw r5, 0(r13)"); println("  sw r14, 4(r13)"); println("  addi r5, r13, 0");
    println("  sw r6, 8(r13)");
    println("  sw r7, 12(r13)");
    println("  sw r8, 16(r13)");
    println("  sw r9, 20(r13)");
    println("  sw r15, 24(r13)");
    if (fn->va_area) {
      /* The incoming register save area belongs above the active frame.
         A callee is allowed to use the caller's current stack area while it
         builds its own frame.  Keeping the save area at old-SP+16 therefore
         matches GCC and prevents vfprintf (and other callees) from
         overwriting the variadic arguments before va_arg consumes them. */
      println("  sw r1, 16(r12)");
      println("  sw r2, 20(r12)");
      println("  sw r3, 24(r12)");
      println("  sw r4, 28(r12)");
    }
    int argreg = 1;
    int stack_word = 0;
    for (Obj *v = fn->params; v; v = v->next) {
      for (int word = 0; word < arg_words(v->ty); word++) {
        if (argreg <= 4)
          store_fp(argreg++, v->offset + word * 4);
        else {
          load_fp(12, 28);
          add_offset(12, 12, 32 + stack_word++ * 4, 2);
          println("  lw r1, 0(r12)");
          store_fp(1, v->offset + word * 4);
        }
      }
    }
    gen_stmt(fn->body);
    if (!strcmp(fn->name, "main")) println("  li r1, 0");
    println(".Lreturn.%s:", fn->name);
    println("  add r13, r5, r0");
    println("  lw r6, 8(r13)");
    println("  lw r7, 12(r13)");
    println("  lw r8, 16(r13)");
    println("  lw r9, 20(r13)");
    println("  lw r15, 24(r13)");
    println("  lw r14, 4(r13)"); println("  lw r5, 0(r13)");
    println("  lw r12, 28(r13)");
    println("  add r13, r12, r0");
    println("  jr r14");
  }
}

void codegen(Obj *prog, FILE *out) {
  output_file = out;
  for (Obj *var = prog; var; var = var->next) {
    if (var->alias_name) {
      Obj *target = prog;
      while (target && strcmp(target->name, var->alias_name))
        target = target->next;
      if (!target || !target->is_definition || target->alias_name)
        error("alias %s requires a definition of %s in the same translation unit",
              var->name, var->alias_name);
      println("  %s %s", var->is_weak ? ".weak" : ".global", var->name);
      println("  .set %s, %s", var->name, var->alias_name);
    } else if (var->is_weak && !var->is_definition) {
      println("  .weak %s", var->name);
    }
  }
  File **files = get_input_files();
  for (int i = 0; files[i]; i++) println("  .file %d \"%s\"", files[i]->file_no, files[i]->name);
  assign_lvar_offsets(prog);
  emit_data(prog);
  emit_text(prog);
}
