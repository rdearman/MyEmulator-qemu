/* REM ABI regression: double is 8 bytes but only 4-byte aligned. */
struct probe {
  char tag;
  double value;
};

int main(void) {
  return sizeof(struct probe) != 12 || _Alignof(struct probe) != 4;
}
