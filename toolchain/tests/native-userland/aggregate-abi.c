struct small { int first, last; };
struct large { long long seconds; int padding, nanos; };
/* REM C ABI: every aggregate result uses a hidden destination pointer,
   including this 8-byte return. Keep this test compatible with REM GCC. */
#ifdef AGGREGATE_EXTERNAL
struct small small_copy(struct small);
struct large large_copy(int, int, int, struct large, int);
#else
struct small small_copy(struct small value) { value.last += 1; return value; }
struct large large_copy(int a, int b, int c, struct large value, int d)
{
  value.seconds += a + b + c + d;
  value.nanos += 1;
  return value;
}
#endif
#ifndef AGGREGATE_NO_MAIN
int main(void)
{
  struct small s = {11, 22};
  struct small t = small_copy(s);
#ifdef AGGREGATE_SMALL_RETURN_ONLY
  if (s.last != 22) return 11;
  if (t.first != 11) return 12;
  if (t.last != 23) return 13;
  return 0;
#else
  struct large l = {10000000000LL, 7, 123};
  struct large m = large_copy(1, 2, 3, l, 4);
  if (s.last != 22) return 11;
  if (t.first != 11) return 12;
  if (t.last != 23) return 13;
  if (l.seconds != 10000000000LL || l.nanos != 123) return 21;
  if (m.seconds != 10000000010LL || m.padding != 7 || m.nanos != 124) return 22;
  if (small_copy(s).last != 23) return 14;
  if (large_copy(1, 2, 3, l, 4).nanos != 124) return 23;
  return 0;
#endif
}
#endif
