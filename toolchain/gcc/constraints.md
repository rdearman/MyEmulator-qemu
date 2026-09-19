(define_constraint "I" "Signed twelve-bit immediate"
  (and (match_code "const_int")
       (match_test "IN_RANGE (ival, -2048, 2047)")))

(define_constraint "K" "Five-bit shift count"
  (and (match_code "const_int")
       (match_test "IN_RANGE (ival, 0, 31)")))

(define_constraint "Z" "fixed frame/stack register"
  (and (match_code "reg")
       (match_test "REGNO (op) == 13 || REGNO (op) == 15 || REGNO (op) == 16")))
