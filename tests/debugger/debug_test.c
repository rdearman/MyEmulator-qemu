#include <stdint.h>
#include <string.h>

static volatile uint32_t result;

__attribute__((noinline)) static uint32_t add_values(uint32_t left,
						    uint32_t right)
{
	volatile uint32_t local = left + right;
	return local;
}

int main(int argc, char **argv)
{
	result = add_values(19, 23);
	/* A real REM BREAK opcode lets the debugger validate exception-to-SIGTRAP
	 * delivery independently from software breakpoints installed by remdbg. */
	__asm__ volatile (".word 0x25000000" ::: "memory");
	if (argc > 1 && !strcmp(argv[1], "fault")) {
		volatile uint32_t *bad = (volatile uint32_t *)(uintptr_t)0xdead0000u;
		result = *bad;
	}
	return result == 42 ? 0 : 1;
}
