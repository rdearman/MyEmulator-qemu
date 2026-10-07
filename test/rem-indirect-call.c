static int add_one(int x) { return x + 1; }
int main(void) {
  int (*fn)(int) = add_one;
  return fn(41) == 42 ? 0 : 1;
}
