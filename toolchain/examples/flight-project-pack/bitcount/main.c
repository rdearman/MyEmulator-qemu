#include <stdio.h>

extern unsigned int bitcount32(unsigned int value);

int main(int argc, char **argv)
{
	FILE *input;
	unsigned long bytes = 0;
	unsigned long bits = 0;
	int c;
	if (argc != 2) {
		fprintf(stderr, "usage: bitcount FILE\n");
		return 2;
	}
	input = fopen(argv[1], "rb");
	if (!input) {
		perror(argv[1]);
		return 1;
	}
	while ((c = fgetc(input)) != EOF) {
		++bytes;
		bits += bitcount32((unsigned int)c);
	}
	if (ferror(input)) {
		perror(argv[1]);
		fclose(input);
		return 1;
	}
	if (fclose(input) != 0) {
		perror(argv[1]);
		return 1;
	}
	printf("bytes=%lu set-bits=%lu\n", bytes, bits);
	if (ferror(stdout))
		return 1;
	return 0;
}
