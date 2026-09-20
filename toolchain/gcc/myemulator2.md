(include "constraints.md")
(include "predicates.md")

(define_attr "length" "" (const_int 4))

(define_insn "nop"
  [(const_int 0)] "" "addi r0, r0, 0")

(define_insn "addsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
        (plus:SI (match_operand:SI 1 "register_operand" "r,r")
                 (match_operand:SI 2 "nonmemory_operand" "r,I")))]
  ""
  "* return CONST_INT_P (operands[2]) && INTVAL (operands[2]) < 0 ? \"subi %0, %1, %n2\" : CONST_INT_P (operands[2]) ? \"addi %0, %1, %2\" : \"add %0, %1, %2\";"
  [(set_attr "length" "4,4")])

(define_insn "subsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
        (minus:SI (match_operand:SI 1 "register_operand" "r,r")
                  (match_operand:SI 2 "nonmemory_operand" "r,I")))]
  ""
  "* return CONST_INT_P (operands[2]) && INTVAL (operands[2]) < 0 ? \"addi %0, %1, %n2\" : CONST_INT_P (operands[2]) ? \"subi %0, %1, %2\" : \"sub %0, %1, %2\";"
  [(set_attr "length" "4,4")])

(define_insn "mulsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (mult:SI (match_operand:SI 1 "register_operand" "r")
                 (match_operand:SI 2 "register_operand" "r")))]
  "" "mul %0, %1, %2")
(define_insn "divsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (div:SI (match_operand:SI 1 "register_operand" "r")
                (match_operand:SI 2 "register_operand" "r")))]
  "" "div %0, %1, %2")
(define_insn "udivsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (udiv:SI (match_operand:SI 1 "register_operand" "r")
                 (match_operand:SI 2 "register_operand" "r")))]
  "" "divu %0, %1, %2")
(define_insn "modsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (mod:SI (match_operand:SI 1 "register_operand" "r")
                (match_operand:SI 2 "register_operand" "r")))]
  "" "rem %0, %1, %2")
(define_insn "umodsi3"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (umod:SI (match_operand:SI 1 "register_operand" "r")
                 (match_operand:SI 2 "register_operand" "r")))]
  "" "remu %0, %1, %2")

(define_insn "andsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
        (and:SI (match_operand:SI 1 "register_operand" "r,r")
                (match_operand:SI 2 "nonmemory_operand" "r,J")))]
  "" "* return CONST_INT_P (operands[2]) ? \"andi %0, %1, %2\" : \"and %0, %1, %2\";")
(define_insn "iorsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
        (ior:SI (match_operand:SI 1 "register_operand" "r,r")
                (match_operand:SI 2 "nonmemory_operand" "r,J")))]
  "" "* return CONST_INT_P (operands[2]) ? \"ori %0, %1, %2\" : \"or %0, %1, %2\";")
(define_insn "xorsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
        (xor:SI (match_operand:SI 1 "register_operand" "r,r")
                (match_operand:SI 2 "nonmemory_operand" "r,J")))]
  "" "* return CONST_INT_P (operands[2]) ? \"xori %0, %1, %2\" : \"xor %0, %1, %2\";")
(define_insn "one_cmplsi2"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (not:SI (match_operand:SI 1 "register_operand" "r")))]
  "" "not %0, %1, r0")

(define_insn "ashlsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
        (ashift:SI (match_operand:SI 1 "register_operand" "r,r")
                   (match_operand:SI 2 "nonmemory_operand" "r,I")))]
  "" "* return CONST_INT_P (operands[2]) ? \"slli %0, %1, %2\" : \"sll %0, %1, %2\";")
(define_insn "lshrsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
        (lshiftrt:SI (match_operand:SI 1 "register_operand" "r,r")
                     (match_operand:SI 2 "nonmemory_operand" "r,I")))]
  "" "* return CONST_INT_P (operands[2]) ? \"srli %0, %1, %2\" : \"srl %0, %1, %2\";")
(define_insn "ashrsi3"
  [(set (match_operand:SI 0 "register_operand" "=r,r")
        (ashiftrt:SI (match_operand:SI 1 "register_operand" "r,r")
                     (match_operand:SI 2 "nonmemory_operand" "r,I")))]
  "" "* return CONST_INT_P (operands[2]) ? \"srai %0, %1, %2\" : \"sra %0, %1, %2\";")

(define_expand "movsi"
  [(set (match_operand:SI 0 "general_operand" "")
        (match_operand:SI 1 "general_operand" ""))]
  ""
  "{ if (MEM_P (operands[0]) && !register_operand (operands[1], SImode))
       operands[1] = force_reg (SImode, operands[1]); }")

(define_insn "*movsi"
  [(set (match_operand:SI 0 "nonimmediate_operand" "=r,r,r,m")
        (match_operand:SI 1 "general_operand" "r,i,m,r"))]
  "register_operand (operands[0], SImode)
   || register_operand (operands[1], SImode)"
  "* switch (which_alternative) { case 0: return \"add %0, %1, r0\"; case 1: return \"li %0, %1\"; case 2: return \"lw %0, %1\"; default: return \"sw %1, %0\"; }")

(define_expand "movqi"
  [(set (match_operand:QI 0 "general_operand" "")
        (match_operand:QI 1 "general_operand" ""))]
  ""
  "{ if (MEM_P (operands[0])) operands[1] = force_reg (QImode, operands[1]); }")

(define_insn "*movqi"
  [(set (match_operand:QI 0 "nonimmediate_operand" "=r,r,r,m")
        (match_operand:QI 1 "general_operand" "r,i,m,r"))]
  "register_operand (operands[0], QImode)
   || register_operand (operands[1], QImode)"
  "* switch (which_alternative) { case 0: return \"add %0, %1, r0\"; case 1: return \"li %0, %1\"; case 2: return \"lb %0, %1\"; default: return \"sb %1, %0\"; }")

(define_expand "movhi"
  [(set (match_operand:HI 0 "general_operand" "")
        (match_operand:HI 1 "general_operand" ""))]
  ""
  "{ if (MEM_P (operands[0])) operands[1] = force_reg (HImode, operands[1]); }")

(define_insn "*movhi"
  [(set (match_operand:HI 0 "nonimmediate_operand" "=r,r,r,m")
        (match_operand:HI 1 "general_operand" "r,i,m,r"))]
  "register_operand (operands[0], HImode)
   || register_operand (operands[1], HImode)"
  "* switch (which_alternative) { case 0: return \"add %0, %1, r0\"; case 1: return \"li %0, %1\"; case 2: return \"lh %0, %1\"; default: return \"sh %1, %0\"; }")

(define_insn "movsi_push"
  [(set (mem:SI (pre_dec:SI (reg:SI 13)))
        (match_operand:SI 0 "register_operand" "r"))]
  ""
  "addi sp, sp, -4\n\tsw %0, 0(sp)"
  [(set_attr "length" "8")])

(define_insn "*myemulator2_movsi_predec"
  [(set (mem:SI (pre_dec:SI (reg:SI 13)))
        (match_operand:SI 0 "register_operand" "r"))]
  ""
  "addi sp, sp, -4\n\tsw %0, 0(sp)"
  [(set_attr "length" "8")])

(define_insn "movsi_pop"
  [(set (match_operand:SI 1 "register_operand" "=r")
        (mem:SI (post_inc:SI (match_operand:SI 0 "register_operand" "r"))))]
  ""
  "lw %1, 0(%0)\n\taddi %0, %0, 4"
  [(set_attr "length" "8")])

(define_insn "*myemulator2_loadsi"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (match_operand:SI 1 "memory_operand" "m"))]
  "" "lw %0, %1")

(define_insn "*myemulator2_loadqi"
  [(set (match_operand:QI 0 "register_operand" "=r")
        (match_operand:QI 1 "memory_operand" "m"))]
  "" "lb %0, %1")
(define_insn "*myemulator2_loadhi"
  [(set (match_operand:HI 0 "register_operand" "=r")
        (match_operand:HI 1 "memory_operand" "m"))]
  "" "lh %0, %1")

(define_insn "*myemulator2_store_si"
  [(set (match_operand:SI 0 "memory_operand" "=m")
        (match_operand:SI 1 "register_operand" "r"))]
  "" "sw %1, %0")
(define_insn "*myemulator2_store_qi"
  [(set (match_operand:QI 0 "memory_operand" "=m")
        (match_operand:QI 1 "register_operand" "r"))]
  "" "sb %1, %0")
(define_insn "*myemulator2_store_hi"
  [(set (match_operand:HI 0 "memory_operand" "=m")
        (match_operand:HI 1 "register_operand" "r"))]
  "" "sh %1, %0")

(define_insn "*myemulator2_extendqisi"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (sign_extend:SI (match_operand:QI 1 "memory_operand" "m")))]
  "" "lb %0, %1")
(define_insn "*myemulator2_extendhisi"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (sign_extend:SI (match_operand:HI 1 "memory_operand" "m")))]
  "" "lh %0, %1")
(define_insn "*myemulator2_zero_extendqisi"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (zero_extend:SI (match_operand:QI 1 "memory_operand" "m")))]
  "" "lbu %0, %1")
(define_insn "*myemulator2_zero_extendhisi"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (zero_extend:SI (match_operand:HI 1 "memory_operand" "m")))]
  "" "lhu %0, %1")

(define_expand "cbranchsi4"
  [(set (pc) (if_then_else (match_operator 0 "comparison_operator"
                           [(match_operand:SI 1 "register_operand")
                            (match_operand:SI 2 "register_operand")])
                          (label_ref (match_operand 3 "" ""))
                          (pc)))] "" "")
(define_insn "*myemulator2_cbranchsi4"
  [(set (pc) (if_then_else (match_operator 0 "comparison_operator"
                           [(match_operand:SI 1 "register_operand" "r")
                            (match_operand:SI 2 "register_operand" "r")])
                          (label_ref (match_operand 3 "" ""))
                          (pc)))] ""
  "* switch (GET_CODE (operands[0])) { case EQ: return \"beq %1, %2, %l3\"; case NE: return \"bne %1, %2, %l3\"; case LT: return \"blt %1, %2, %l3\"; case GE: return \"bge %1, %2, %l3\"; case LTU: return \"bltu %1, %2, %l3\"; case GEU: return \"bgeu %1, %2, %l3\"; case GT: return \"blt %2, %1, %l3\"; case LE: return \"bge %2, %1, %l3\"; case GTU: return \"bltu %2, %1, %l3\"; case LEU: return \"bgeu %2, %1, %l3\"; default: gcc_unreachable (); }")

(define_expand "cbranchdf4"
  [(set (pc) (if_then_else
               (match_operator 0 "comparison_operator"
                 [(match_operand:DF 1 "register_operand")
                  (match_operand:DF 2 "register_operand")])
               (label_ref (match_operand 3 "" ""))
               (pc)))]
  ""
  "{ myemulator2_expand_cbranchdf4 (operands); DONE; }")

(define_expand "cstoresi4"
  [(set (match_operand:SI 0 "register_operand")
        (match_operator:SI 1 "ordered_comparison_operator"
          [(match_operand:SI 2 "register_operand")
           (match_operand:SI 3 "nonmemory_operand")]))]
  ""
  "{ if (!register_operand (operands[3], SImode))
       operands[3] = force_reg (SImode, operands[3]); }")

(define_insn "*myemulator2_cstoresi4"
  [(set (match_operand:SI 0 "register_operand" "=r")
        (match_operator:SI 1 "ordered_comparison_operator"
          [(match_operand:SI 2 "register_operand" "r")
           (match_operand:SI 3 "register_operand" "r")]))]
  ""
  "* switch (GET_CODE (operands[1])) { case EQ: return \"seq %0, %2, %3\"; case NE: return \"sne %0, %2, %3\"; case LT: return \"slt %0, %2, %3\"; case GE: return \"sge %0, %2, %3\"; case LTU: return \"sltu %0, %2, %3\"; case GEU: return \"sgeu %0, %2, %3\"; case GT: return \"slt %0, %3, %2\"; case LE: return \"sge %0, %3, %2\"; case GTU: return \"sltu %0, %3, %2\"; case LEU: return \"sgeu %0, %3, %2\"; default: gcc_unreachable (); }")

(define_insn "jump"
  [(set (pc) (label_ref (match_operand 0 "" "")))] "" "j %l0")
(define_insn "indirect_jump"
  [(set (pc) (match_operand:SI 0 "register_operand" "r"))] "" "jr %0")
(define_expand "call"
  [(call (match_operand:QI 0 "memory_operand" "")
         (match_operand 1 "general_operand" ""))]
  ""
  "{ gcc_assert (MEM_P (operands[0])); }")
(define_insn "*call"
  [(call (mem:QI (match_operand:SI 0 "nonmemory_operand" "i,r"))
         (match_operand 1 "" ""))]
  ""
  "@
   jal %0
   jalr %0")
(define_expand "call_value"
  [(set (match_operand 0 "" "")
        (call (match_operand:QI 1 "memory_operand" "")
              (match_operand 2 "" "")))]
  ""
  "{ gcc_assert (MEM_P (operands[1])); }")
(define_insn "*call_value"
  [(set (match_operand 0 "register_operand" "=r")
        (call (mem:QI (match_operand:SI 1 "immediate_operand" "i"))
              (match_operand 2 "" "")))] "" "jal %1")
(define_insn "*call_value_indirect"
  [(set (match_operand 0 "register_operand" "=r")
        (call (mem:QI (match_operand:SI 1 "register_operand" "r"))
              (match_operand 2 "" "")))] "" "jalr %1")
(define_insn "returner" [(return)] "reload_completed" "jr lr")

(define_expand "prologue" [(clobber (const_int 0))] "" "{ myemulator2_expand_prologue (); DONE; }")
(define_expand "epilogue" [(return)] "" "{ myemulator2_expand_epilogue (); DONE; }")
