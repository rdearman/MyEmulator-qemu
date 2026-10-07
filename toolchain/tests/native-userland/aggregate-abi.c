struct small { int first, last; };
struct large { long long seconds; int padding, nanos; };
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
  struct large l = {10000000000LL, 7, 123};
  struct large m = large_copy(1, 2, 3, l, 4);
  if (s.last != 22 || t.first != 11 || t.last != 23) return 1;
  if (l.seconds != 10000000000LL || l.nanos != 123) return 2;
  if (m.seconds != 10000000010LL || m.padding != 7 || m.nanos != 124) return 3;
  if (small_copy(s).last != 23) return 4;
  if (large_copy(1, 2, 3, l, 4).nanos != 124) return 5;
  return 0;
}
#endif
