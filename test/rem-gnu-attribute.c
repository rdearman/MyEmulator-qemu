static int ignored(int value __attribute__((unused))) {
  return 42;
}

int main(void) {
  return ignored(7) == 42 ? 0 : 1;
}
