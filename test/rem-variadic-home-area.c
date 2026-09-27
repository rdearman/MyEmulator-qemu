#include <stdio.h>

/* A variadic call with only register arguments still needs the REM outgoing
   home area.  The callee uses it for its incoming-register save area. */
int main(void) {
  printf("%d %d\n", 1, 2);
  return 0;
}
