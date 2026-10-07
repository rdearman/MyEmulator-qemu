int main(void) {
  volatile unsigned long long high = 1ULL << 63;
  if (!(high > 0x7fffffffffffffff)) return 1;
  if (sizeof(0x100000000) != sizeof(long long)) return 2;
  if (!(0xffffffffffffffff >= high)) return 3;
  if (!(0x80000000L > 0)) return 4;
  if (!(0xffffffffffffffffLL > 0)) return 5;
  return 0;
}
