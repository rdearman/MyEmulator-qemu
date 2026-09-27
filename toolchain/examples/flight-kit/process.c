#include <stdio.h>

/*
 * Process project starter.
 * REM exposes clone (220), execve (221), and wait4 (260), not fork (57).
 * Design and validate a small wrapper for the current clone ABI before adding
 * code here. A portable project can put that wrapper behind spawn_child().
 */
int main(void)
{
	puts("TODO: implement spawn_child() for this target and test exec/wait");
	return 0;
}
