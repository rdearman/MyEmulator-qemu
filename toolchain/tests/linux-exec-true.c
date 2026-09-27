#include <myemu/string.h>
#include <myemu/syscall.h>

int main(void)
{
	static const char path[] = "/usr/bin/true";
	static const char *const argv[] = { path, 0 };
	static const char *const envp[] = { 0 };
	static const char failed[] = "EXEC_TRUE_FAILED\n";

	myemu_execve(path, argv, envp);
	myemu_write(1, failed, myemu_strlen(failed));
	return 1;
}
