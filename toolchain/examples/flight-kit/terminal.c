#include <stdio.h>
#include <unistd.h>

/* A line-oriented starting point that works without cursor-control escapes. */
int main(void)
{
	char line[256];
	puts("REM notes: enter a line (Ctrl-D to finish)");
	while (fgets(line, sizeof(line), stdin)) {
		if (fputs("you wrote: ", stdout) == EOF || fputs(line, stdout) == EOF)
			return 1;
	}
	return ferror(stdin) ? 1 : 0;
}
