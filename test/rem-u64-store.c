#include <stdint.h>

/* Regression: assigning a plain uint64_t local must keep the destination
 * address separate from the high value word (r2) while storing r1:r2. */
int main(void) {
  uint64_t x = 0;
  x = 0xbdd89aa982704029ull;
  return x == 0xbdd89aa982704029ull ? 0 : 1;
}
