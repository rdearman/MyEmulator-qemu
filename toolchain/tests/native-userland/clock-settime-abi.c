#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(void)
{
  long long stamp[2] = {1791360000LL, 0};
  struct timespec actual;
  long result = syscall(SYS_clock_settime64, CLOCK_REALTIME, stamp);
  if (result != 0) {
    perror("clock_settime64");
    return 1;
  }
  if (clock_gettime(CLOCK_REALTIME, &actual) != 0) {
    perror("clock_gettime");
    return 1;
  }
  if (actual.tv_sec < stamp[0] || actual.tv_sec > stamp[0] + 60) {
    fprintf(stderr, "clock_settime64 did not update CLOCK_REALTIME\n");
    return 1;
  }
  puts("CLOCK_SETTIME64_OK");
  return 0;
}
