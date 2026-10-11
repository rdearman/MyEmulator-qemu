#include <stdint.h>
#include <string.h>

static volatile uint32_t result;

#ifdef REMDBG_TEST_EXPLICIT_BREAK
static void explicit_break(void)
{
	__asm__ volatile(".word 0x25000000");
}
#else
static void explicit_break(void) {}
#endif

static uint32_t add_values(uint32_t left, uint32_t right)
{
	volatile uint32_t local = left + right;
	return local;
}

#ifdef REMDBG_TEST_SELF_BRANCH
/* A taken, non-linking jump whose target is its own instruction address.
 * The debugger qualification steps this instruction to cover the legal
 * current-PC successor case in the kernel's temporary-breakpoint stepper. */
__attribute__((noinline)) void self_branch(void)
{
	__asm__ volatile(".word 0x17ffffff"); /* j . */
}

__attribute__((noinline)) void self_link_branch(void)
{
	__asm__ volatile(".word 0x1bffffff"); /* jal . */
}
#endif

static uint32_t deliberate_fault(void)
{
	volatile uint32_t *bad = (volatile uint32_t *)(uintptr_t)0xdead0000u;
	return *bad;
}

int main(int argc, char **argv)
{
#ifdef REMDBG_TEST_SELF_BRANCH
	if (argc > 1 && !strcmp(argv[1], "self-step")) {
		self_branch();
		return 0;
	}
	if (argc > 1 && !strcmp(argv[1], "self-link-step")) {
		self_link_branch();
		return 0;
	}
#endif
	result = add_values(19, 23);
	if (argc == 1)
		explicit_break();
	/* Exercise the debugger's software breakpoint with `break main`. Keeping
	 * the test in portable C also lets it be built by both REM GCC and chibicc. */
	if (argc > 1 && !strcmp(argv[1], "fault")) {
		result = deliberate_fault();
	}
	return result == 42 ? 0 : 1;
}
