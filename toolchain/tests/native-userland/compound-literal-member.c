int main(void) {
  if ((union { int value; char byte; }){.value = 42}.value != 42)
    return 1;
  if ((struct { int value; }){42}.value != 42)
    return 2;
  return (int[]){40, 42}[1] != 42;
}
