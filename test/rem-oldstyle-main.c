/* An empty parameter list is an old-style unspecified prototype, not `...`.
   In particular, defining main() must not allocate a variadic register-save
   area or write outside its stack frame. */
int main() {
  return 0;
}
