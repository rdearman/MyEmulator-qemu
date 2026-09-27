#define _GNU_SOURCE
#include <errno.h>
#include <sys/random.h>
#include <unistd.h>

int
main (void)
{
  unsigned char byte;
  ssize_t n = getrandom (&byte, sizeof byte, GRND_NONBLOCK);
  if (n == 1)
    {
      write (1, "GETRANDOM_READY\n", 16);
      return 0;
    }
  if (n < 0 && errno == EAGAIN)
    {
      write (1, "GETRANDOM_NOT_READY\n", 20);
      return 2;
    }
  write (1, "GETRANDOM_OTHER_RESULT\n", 23);
  return 1;
}
