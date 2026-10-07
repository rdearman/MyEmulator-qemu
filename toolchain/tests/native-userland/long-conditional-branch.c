#define STEP value++;
#define FOUR STEP STEP STEP STEP
#define SIXTEEN FOUR FOUR FOUR FOUR
#define SIXTYFOUR SIXTEEN SIXTEEN SIXTEEN SIXTEEN
#define MANY SIXTYFOUR SIXTYFOUR SIXTYFOUR SIXTYFOUR
static int check(int mode)
{
  volatile int value = 0;
  if (mode) { MANY MANY MANY MANY }
  return value;
}
int main(void) { return check(0) != 0 || check(1) != 1024; }
