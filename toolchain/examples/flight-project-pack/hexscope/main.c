#include "hexscope.h"
#include <string.h>

int main(int argc, char **argv)
{
	int elf32 = 0;
	const char *path;
	FILE *input;
	int result;
	if (argc == 3 && strcmp(argv[1], "--elf32") == 0) {
		elf32 = 1;
		path = argv[2];
	} else if (argc == 2) {
		path = argv[1];
	} else {
		fprintf(stderr, "usage: hexscope [--elf32] FILE\n");
		return 2;
	}
	input = fopen(path, "rb");
	if (!input) {
		perror(path);
		return 1;
	}
	if (elf32)
		result = inspect_elf32(input);
	else
		result = dump_bytes(input);
	if (fclose(input) != 0) {
		perror(path);
		return 1;
	}
	if (result == 0)
		return 0;
	return 1;
}
