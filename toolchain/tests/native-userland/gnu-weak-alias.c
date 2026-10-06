int original(int n) { return n + 1; }
extern __typeof__(original) alternate
    __attribute__((weak, alias("original")));
extern int optional(void) __attribute__((weak));

int main(void) {
  if (alternate(41) != 42 || alternate != original)
    return 1;
  return optional != 0;
}
