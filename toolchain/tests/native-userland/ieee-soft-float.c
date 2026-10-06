#include <stdio.h>

static unsigned long long double_bits(double value) {
  union {
    double value;
    unsigned long long bits;
  } rep;
  rep.value = value;
  return rep.bits;
}

static double from_double_bits(unsigned long long bits) {
  union {
    double value;
    unsigned long long bits;
  } rep;
  rep.bits = bits;
  return rep.value;
}

static unsigned float_bits(float value) {
  union {
    float value;
    unsigned bits;
  } rep;
  rep.value = value;
  return rep.bits;
}

static float from_float_bits(unsigned bits) {
  union {
    float value;
    unsigned bits;
  } rep;
  rep.bits = bits;
  return rep.value;
}

int main(void) {
  volatile int three = 3;
  volatile double a = 1.5, b = 2.25, zero = 0.0;
  volatile float fa = 1.5f, fb = 2.25f;
  volatile unsigned long long wide = 1099511627779ULL;
  volatile long long negative = -1099511627779LL;
  volatile double nan = from_double_bits(0x7ff8000000000001ULL);
  volatile double tiny = from_double_bits(1ULL);
  volatile float ftiny = from_float_bits(1U);
  volatile float fnan = from_float_bits(0x7fc00001U);
  volatile double minus_zero = from_double_bits(0x8000000000000000ULL);
  volatile double minimum = -0x1p63;

  if (double_bits((double)three / 2) != 0x3ff8000000000000ULL)
    return 1;
  if (a + b != 3.75 || b - a != 0.75 || a * b != 3.375 || b / a != 1.5)
    return 2;
  if (float_bits(fa + fb) != 0x40700000U ||
      fa * fb != 3.375f || fb / fa != 1.5f)
    return 3;
  if ((double)fa != a || (float)b != fb)
    return 4;
  if ((int)-a != -1 || (unsigned)b != 2U ||
      (long long)(double)negative != negative ||
      (unsigned long long)(double)wide != wide ||
      (long long)minimum != (-9223372036854775807LL - 1))
    return 5;
  if ((double)wide != 1099511627779.0 ||
      (double)negative != -1099511627779.0 ||
      float_bits((float)wide) != 0x53800000U)
    return 6;
  if (nan == nan || nan < zero || nan <= zero ||
      fnan == fnan || fnan < fa || fnan <= fa)
    return 7;
  if (!(_Bool)nan || (_Bool)minus_zero || minus_zero != zero)
    return 8;
  if (double_bits(tiny + tiny) != 2ULL ||
      double_bits(tiny * 2.0) != 2ULL ||
      float_bits(ftiny + ftiny) != 2U)
    return 9;
  if (double_bits(a + minus_zero) != double_bits(a) ||
      double_bits(minus_zero * a) != 0x8000000000000000ULL)
    return 10;
  puts("IEEE_SOFT_FLOAT_OK");
  return 0;
}
