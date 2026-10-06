#include <stdbool.h>
#include <stdint.h>

static _Alignas(256) unsigned char buffer[256];

static bool as_bool(void *p) { return p; }

int main(void) {
  void *volatile pointer = buffer;
  volatile unsigned word = 256;
  volatile uint64_t wide = 1ULL << 32;
  bool p = pointer;
  bool n = word;
  bool w = wide;
  if (!p || p != 1 || !n || n != 1 || !w || w != 1)
    return 1;
  return as_bool(pointer) != 1;
}
