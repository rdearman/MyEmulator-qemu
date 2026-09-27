#include <stdio.h>
#include <time.h>

int main(void)
{
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
		perror("clock_gettime");
		return 1;
	}
	printf("monotonic %ld seconds + %ld ns\n", now.tv_sec, now.tv_nsec);
	return 0;
}
