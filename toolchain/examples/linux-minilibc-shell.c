#include <myemu/string.h>
#include <myemu/syscall.h>

static char line[128];

static void put(const char *text)
{
	myemu_write(1, text, myemu_strlen(text));
}

static int is_command(const char *text, const char *command)
{
	while (*command && *text == *command) {
		text++;
		command++;
	}
	return *command == 0 && (*text == 0 || *text == ' ');
}

int main(void)
{
	put("myemu-c> ");
	for (;;) {
		long length = 0;
		for (;;) {
			long count = myemu_read(0, line + length,
						 sizeof(line) - 1 - length);
			if (count <= 0)
				continue;
			length += count;
			line[length] = 0;
			if (line[length - 1] == '\n' || line[length - 1] == '\r')
				break;
		}
		while (length && (line[length - 1] == '\n' ||
					 line[length - 1] == '\r'))
			line[--length] = 0;
		if (is_command(line, "help"))
			put("help echo uname mem exit\r\n");
		else if (is_command(line, "echo")) {
			const char *text = line + 4;
			while (*text == ' ')
				text++;
			myemu_write(1, text, myemu_strlen(text));
			put("\r\n");
		} else if (is_command(line, "uname"))
			put("REM Linux 6.12.1\r\n");
		else if (is_command(line, "mem"))
			put("REM userspace memory OK\r\n");
		else if (is_command(line, "exit"))
			myemu_exit(0);
		else
			put("unknown command\r\n");
		put("myemu-c> ");
	}
}
