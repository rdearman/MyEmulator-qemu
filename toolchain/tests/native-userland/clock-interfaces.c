#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>

int main(void)
{
  struct timespec now;
  struct stat st;
  int failed = 0;
  if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
    perror("clock_gettime");
    return 1;
  }
  printf("CLOCK_REALTIME=%lld\n", (long long)now.tv_sec);
  const int numbers[] = {112, 170, 171, 266, 404, 405};
  const char *names[] = {"clock_settime32", "settimeofday", "adjtimex",
                        "clock_adjtime32", "clock_settime64", "clock_adjtime64"};
  for (unsigned i = 0; i < sizeof(numbers) / sizeof(numbers[0]); ++i) {
    errno = 0;
    long result = syscall(numbers[i], 0, 0, 0);
    printf("%s nr=%d result=%ld errno=%d\n",
           names[i], numbers[i], result, errno);
    if (result < 0 && errno != ENOSYS && errno != EFAULT && errno != EPERM)
      failed = 1;
  }
  printf("RTC_DEVICE=%s\n", stat("/dev/rtc", &st) == 0 ||
         stat("/dev/rtc0", &st) == 0 ? "present" : "absent");
  return failed;
}
