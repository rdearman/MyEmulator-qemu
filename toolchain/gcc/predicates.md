;; The initial backend uses GCC's generic register, memory and immediate
;; predicates.  This file is retained as the target extension point.
(define_predicate "myemulator2_register_operand"
  (match_code "reg,subreg"))

(define_predicate "myemulator2_reg_or_const"
  (match_code "reg,subreg,const_int,const_double,const,const_wide_int,symbol_ref,label_ref"))

(define_predicate "myemulator2_nonimmediate"
  (ior (match_code "reg,subreg")
       (and (match_code "mem")
            (match_test "GET_CODE (XEXP (op, 0)) != PRE_DEC
                         && GET_CODE (XEXP (op, 0)) != POST_INC
                         && GET_CODE (XEXP (op, 0)) != PRE_INC
                         && GET_CODE (XEXP (op, 0)) != POST_DEC"))))
