#include <alloca.h>
#include <stdint.h>
static int sum(int a, int b) { return a + b; }
static int nested(int n)
{
  char *first = alloca(n);
  char *second = alloca(n + 3);
  if (((uintptr_t)first | (uintptr_t)second) & 15) return 1;
  first[0] = 11;
  first[n - 1] = 12;
  second[0] = 13;
  if (sum(20, ((char *)alloca(n + 7))[0] = 22) != 42) return 2;
  return first[0] == 11 && first[n - 1] == 12 && second[0] == 13 ? 0 : 3;
}
int main(void) { return nested(17); }
