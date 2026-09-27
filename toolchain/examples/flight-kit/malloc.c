#include <stdio.h>
#include <stdlib.h>

/* TODO: add overflow checks and growable storage as an allocator exercise. */
int main(void)
{
	int *items = malloc(16 * sizeof(*items));
	int i;
	if (!items) { perror("malloc"); return 1; }
	for (i = 0; i < 16; i++) items[i] = i * i;
	printf("items[15]=%d\n", items[15]);
	free(items);
	return 0;
}
