#include <stdint.h>
#include <stdio.h>

int main(void) {
  volatile int three = 3, two = 2;
  union { double value; uint64_t bits; } number;
  number.value = (double)three / (double)two;
  printf("%08x%08x\n", (unsigned)(number.bits >> 32), (unsigned)number.bits);
  return number.bits != 0x3ff8000000000000ULL;
}
