#include <myemu/string.h>
#include <myemu/syscall.h>

int main(void)
{
	static const char message[] = "MyEmulator2 C minilibc reached\n";
	static const char pid_message[] = "MyEmulator2 C syscall wrappers reached\n";
	if (myemu_write(1, message, myemu_strlen(message)) < 0)
		return 1;
	if (myemu_getpid() < 1)
		return 2;
	if (myemu_write(1, pid_message, myemu_strlen(pid_message)) < 0)
		return 3;
	return 0;
}
