#include <myemu/string.h>
#include <myemu/syscall.h>

static const char path[] = "/root/myemu-persist.txt";
static const char pattern[] = "REM ext4 persistence\n";

static void say(const char *message)
{
	myemu_write(1, message, myemu_strlen(message));
}

int main(void)
{
	char data[sizeof(pattern)];
	int fd = myemu_openat(-100, path, 0, 0);
	unsigned int i;

	if (fd < 0) {
		fd = myemu_openat(-100, path, 0102, 0644);
		if (fd < 0) {
			say("PERSIST OPEN FAIL\n");
			return 1;
		}
		if (myemu_write(fd, pattern, sizeof(pattern) - 1) !=
				(long)(sizeof(pattern) - 1)) {
			say("PERSIST WRITE FAIL\n");
			return 1;
		}
		if (myemu_fsync(fd) < 0) {
			say("PERSIST FSYNC FAIL\n");
			return 1;
		}
		myemu_close(fd);
		say("PERSIST CREATED\n");
		return 0;
	}
	if (myemu_read(fd, data, sizeof(pattern) - 1) != (long)(sizeof(pattern) - 1)) {
		say("PERSIST READ FAIL\n");
		return 2;
	}
	myemu_close(fd);
	for (i = 0; i < sizeof(pattern) - 1; i++)
		if (data[i] != pattern[i]) {
			say("PERSIST DATA FAIL\n");
			return 3;
		}
	say("PERSIST PASS\n");
	return 0;
}
