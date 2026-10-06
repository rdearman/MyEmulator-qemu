#include <stdbool.h>

int main(void) {
  volatile int three = 3, two = 2, nine = 9, four = 4, zero = 0;
  double a = (double)three / (double)two;
  double b = (double)nine / (double)four;
  double result = a + b;
  if ((int)(result * (double)four) != 15)
    return 1;
  if (!(a < b) || a == b || a >= b)
    return 2;
  double negative_zero = -(double)zero;
  bool truth = negative_zero;
  return truth != 0;
}
