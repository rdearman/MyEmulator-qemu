/* A four-argument call must not let the callee's register home area
 * overwrite the caller's locals. */
static int sum4(int a, int b, int c, int d) {
  return a + b + c + d;
}

int main(void) {
  int a = 1;
  int b = 2;
  int c = 3;
  int d = 4;
  int result = sum4(a, b, c, d);
  return result != 10 || a != 1 || b != 2 || c != 3 || d != 4;
}
