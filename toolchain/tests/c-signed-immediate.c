/* Negative constants must use the subtract-immediate encoding.  MyEmulator2
   zero-extends logical/immediate fields, so printing -4 as an ADDI immediate
   would encode 4092 and corrupt the result. */

__attribute__((noinline)) int
myemu2_add_negative (int value)
{
  volatile int input = value;
  return input + (-4);
}

__attribute__((noinline)) int
myemu2_sub_negative (int value)
{
  volatile int input = value;
  return input - (-4);
}
