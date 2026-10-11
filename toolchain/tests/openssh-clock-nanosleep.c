/* Compare musl's time64 clock_nanosleep path with legacy nanosleep. */
#include <errno.h>
#include <stdio.h>
#include <time.h>

int
main(void)
{
	struct timespec req = { .tv_sec = 0, .tv_nsec = 1000000 };
	int rc, saved_errno;

	errno = 0;
	rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL);
	saved_errno = errno;
	printf("CLOCK_NANOSLEEP rc=%d errno=%d\n", rc, saved_errno);
	errno = 0;
	rc = nanosleep(&req, NULL);
	saved_errno = errno;
	printf("NANOSLEEP rc=%d errno=%d\n", rc, saved_errno);
	return 0;
}
