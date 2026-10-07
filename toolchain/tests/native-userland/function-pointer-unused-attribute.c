static void annotated(void (*fn)(void) __attribute__((unused))) {
}

int main(void) {
  annotated(0);
  return 0;
}
