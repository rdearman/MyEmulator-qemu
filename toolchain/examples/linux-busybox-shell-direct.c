#include <myemu/string.h>
#include <myemu/syscall.h>

int main(void)
{
	static const char start[] = "BUSYBOX SHELL DIRECT START\n";
	static const char path[] = "/bin/busybox";
	static const char applet[] = "sh";
	static const char interactive[] = "-i";
	static const char *const argv[] = { path, applet, interactive, 0 };
	static const char *const envp[] = { 0 };
	myemu_write(1, start, myemu_strlen(start));
	myemu_execve(path, argv, envp);
	return 1;
}
