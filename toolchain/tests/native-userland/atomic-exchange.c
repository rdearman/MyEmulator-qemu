#include <stdatomic.h>

int main(void) {
  atomic_int value = 0;
  int old = atomic_exchange_explicit(&value, 4, memory_order_acquire);
  return old != 0 || atomic_load_explicit(&value, memory_order_relaxed) != 4;
}
