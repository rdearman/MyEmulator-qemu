#include <stdio.h>

extern int add_two(int, int);

int main(void)
{
	printf("add_two(19, 23) = %d\n", add_two(19, 23));
	return 0;
}
