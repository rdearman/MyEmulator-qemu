#include <myemu/string.h>
#include <myemu/syscall.h>

int main(void)
{
	static const char start[] = "BUSYBOX DIRECT START\n";
	static const char path[] = "/bin/busybox";
	static const char applet[] = "true";
	static const char *const argv[] = { path, applet, 0 };
	static const char *const envp[] = { 0 };
	static const char error[] = "BUSYBOX EXEC FAIL\n";

	myemu_write(1, start, myemu_strlen(start));
	myemu_execve(path, argv, envp);
	myemu_write(1, error, myemu_strlen(error));
	return 1;
}
