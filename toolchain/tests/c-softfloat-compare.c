/* Compile-only coverage for soft-float comparisons on MyEmulator2.
   The target has no floating-point compare instruction; GCC must lower
   these branches through the libgcc comparison helpers. */

volatile double myemu2_softfloat_value;

int
myemu2_softfloat_compare (double x, double y)
{
  int result = 0;
  if (x == y)
    result += 1;
  if (x != y)
    result += 2;
  if (x < y)
    result += 4;
  if (x <= y)
    result += 8;
  if (x > y)
    result += 16;
  if (x >= y)
    result += 32;
  return result;
}

double
myemu2_softfloat_arithmetic (double x)
{
  return x * 1.5 + 0.5;
}

int
myemu2_softfloat_specials (void)
{
  double infinity = 1.0 / 0.0;
  double nan_value = infinity - infinity;
  myemu2_softfloat_value = nan_value;
  return myemu2_softfloat_compare (nan_value, infinity);
}
