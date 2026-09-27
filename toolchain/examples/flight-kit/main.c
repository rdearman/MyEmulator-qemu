#include <stdio.h>

extern int report_value(void);

int main(void)
{
	printf("report value: %d\n", report_value());
	return 0;
}
