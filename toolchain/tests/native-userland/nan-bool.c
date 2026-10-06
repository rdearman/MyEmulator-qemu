int main(void) {
  union {
    unsigned long long bits;
    double value;
  } rep;
  rep.bits = 0x7ff8000000000001ULL;
  volatile double nan = rep.value;
  return (_Bool)nan ? 0 : 1;
}
