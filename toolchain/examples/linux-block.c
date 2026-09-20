#include <myemu/string.h>
#include <myemu/syscall.h>

static void say(const char *message)
{
	myemu_write(1, message, myemu_strlen(message));
}

int main(void)
{
	static unsigned char pattern[512] __attribute__((aligned(512)));
	static unsigned char result[512] __attribute__((aligned(512)));
	int fd;
	long n;
	unsigned int i;

	for (i = 0; i < sizeof(pattern); i++)
		pattern[i] = (unsigned char)(i ^ 0xa5);
	fd = myemu_openat(-100, "/dev/myemu0", 2 | 040000, 0);
	if (fd < 0) {
		say("BLOCK OPEN FAIL\n");
		return 1;
	}
	if (myemu_lseek(fd, 512, 0) < 0) {
		say("BLOCK SEEK FAIL\n");
		return 2;
	}
	n = myemu_write(fd, pattern, sizeof(pattern));
	if (n != sizeof(pattern)) {
		say("BLOCK WRITE FAIL\n");
		return 3;
	}
	if (myemu_lseek(fd, 512, 0) < 0) {
		say("BLOCK SEEK2 FAIL\n");
		return 4;
	}
	n = myemu_read(fd, result, sizeof(result));
	if (n != sizeof(result)) {
		say("BLOCK READ FAIL\n");
		return 5;
	}
	for (i = 0; i < sizeof(result); i++) {
		if (result[i] != pattern[i]) {
			say("BLOCK DATA FAIL\n");
			return 6;
		}
	}
	say("BLOCK RW PASS\n");
	for (;;)
		myemu_sched_yield();
}
