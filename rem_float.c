#include "chibicc.h"
#ifdef REM_BOOTSTRAP_FLOAT
#include "../ryu/ryu/ryu_parse.h"
#endif

/* Calling through integer declarations preserves the raw ABI words even
 * when the initial native compiler cannot lower floating casts yet. */
#ifdef __myemulator2__
extern unsigned __truncdfsf2(uint64_t);
#endif

void rem_float_bits(const double *value, uint32_t *bits) {
#ifdef __myemulator2__
  uint64_t raw;
  memcpy(&raw, value, sizeof(raw));
  *bits = __truncdfsf2(raw);
#else
  float converted = *value;
  memcpy(bits, &converted, sizeof(converted));
#endif
}

#ifdef REM_BOOTSTRAP_FLOAT
static int hex_digit(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static uint64_t parse_hex(const char *text, int length) {
  uint64_t mantissa = 0;
  int digits = 0, discarded = 0, fraction = 0;
  bool point = false, sticky = false;
  int i = 2;
  for (; i < length && text[i] != 'p' && text[i] != 'P'; i++) {
    if (text[i] == '.' && !point) { point = true; continue; }
    int digit = hex_digit(text[i]);
    if (digit < 0) error("invalid hexadecimal floating literal");
    if (point) fraction++;
    if (!digits && !digit) continue;
    if (digits < 16) {
      mantissa = (mantissa << 4) | digit;
      digits++;
    } else {
      discarded++;
      sticky |= digit != 0;
    }
  }
  if (i == length) error("hexadecimal floating literal requires an exponent");
  i++;
  bool negative = false;
  if (i < length && (text[i] == '+' || text[i] == '-'))
    negative = text[i++] == '-';
  if (i == length) error("missing hexadecimal floating exponent");
  int exponent = 0;
  for (; i < length; i++) {
    if (text[i] < '0' || text[i] > '9') error("invalid floating exponent");
    if (exponent < 100000) exponent = exponent * 10 + text[i] - '0';
  }
  if (!mantissa) return 0;
  if (negative) exponent = -exponent;
  exponent += 4 * (discarded - fraction);
  int bits = 0;
  for (uint64_t n = mantissa; n; n >>= 1) bits++;
  int unbiased = exponent + bits - 1;
  if (unbiased > 1023) return 0x7ff0000000000000ULL;
  int shift = bits - 53;
  if (unbiased < -1022) shift += -1022 - unbiased;
  uint64_t rounded;
  if (shift > 64) rounded = 0;
  else if (shift == 64)
    rounded = (mantissa >> 63) && ((mantissa << 1) || sticky);
  else if (shift > 0) {
    uint64_t half = 1ULL << (shift - 1);
    uint64_t remainder = mantissa & ((1ULL << shift) - 1);
    rounded = mantissa >> shift;
    if (remainder > half || (remainder == half && (sticky || (rounded & 1))))
      rounded++;
  } else
    rounded = mantissa << -shift;
  if (unbiased < -1022) return rounded;
  if (rounded == (1ULL << 53)) { rounded >>= 1; unbiased++; }
  if (unbiased > 1023) return 0x7ff0000000000000ULL;
  return ((uint64_t)(unbiased + 1023) << 52) | (rounded & ((1ULL << 52) - 1));
}
#endif

void rem_parse_float(const char *text, int length, double *value) {
#ifdef REM_INTEGER_BOOTSTRAP
  error("integer bootstrap cannot compile floating literals; build the next native compiler stage");
#elif defined(REM_BOOTSTRAP_FLOAT)
  if (length > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    uint64_t bits = parse_hex(text, length);
    memcpy(value, &bits, sizeof(bits));
    return;
  }
  enum Status status = s2d_n(text, length, value);
  if (status != SUCCESS)
    error("bootstrap decimal conversion failed (Ryu status %d); full libc bootstrap is required", status);
#else
  char *copy = strndup(text, length);
  char *end;
  *value = strtod(copy, &end);
  if (end != copy + length) error("invalid floating-point literal");
  free(copy);
#endif
}
