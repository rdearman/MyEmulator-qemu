#include <stdio.h>
#include <string.h>

int main(void) {
  char output[128];
  snprintf(output, sizeof(output), "%d %ld %u %llu", -36, -36L, 4095U,
           0x1122334455667788ULL);
  puts(output);
  if (strcmp(output, "-36 -36 4095 1234605616436508552")) return 1;
  return 0;
}
