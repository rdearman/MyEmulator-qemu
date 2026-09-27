#include "hexscope.h"

int dump_bytes(FILE *input)
{
	unsigned char bytes[16];
	unsigned int offset_low = 0;
	unsigned int offset_high = 0;
	int failed = 0;
	for (;;) {
		unsigned int count = 0;
		unsigned int i;
		int c;
		while (count < 16 && (c = fgetc(input)) != EOF)
			bytes[count++] = (unsigned char)c;
		if (count == 0)
			break;
		if (offset_high)
			printf("%08x%08x  ", offset_high, offset_low);
		else
			printf("%08x  ", offset_low);
		for (i = 0; i < 16; ++i) {
			if (i < count)
				printf("%02x ", (unsigned int)bytes[i]);
			else
				printf("   ");
			if (i == 7)
				putchar(' ');
		}
		putchar('|');
		for (i = 0; i < count; ++i) {
			if (bytes[i] >= 32 && bytes[i] <= 126)
				putchar(bytes[i]);
			else
				putchar('.');
		}
		puts("|");
		if (ferror(stdout)) {
			failed = 1;
			break;
		}
		{
			unsigned int previous = offset_low;
			offset_low += count;
			if (offset_low < previous)
				++offset_high;
		}
		if (count < 16)
			break;
	}
	if (ferror(input))
		failed = 1;
	if (failed)
		return -1;
	return 0;
}
