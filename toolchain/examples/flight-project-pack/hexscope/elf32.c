#include "hexscope.h"

static unsigned int read16(const unsigned char *p, int little)
{
	if (little)
		return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
	return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static unsigned long read32(const unsigned char *p, int little)
{
	if (little)
		return (unsigned long)p[0] |
		       ((unsigned long)p[1] << 8) |
		       ((unsigned long)p[2] << 16) |
		       ((unsigned long)p[3] << 24);
	return ((unsigned long)p[0] << 24) |
	       ((unsigned long)p[1] << 16) |
	       ((unsigned long)p[2] << 8) |
	       (unsigned long)p[3];
}

int inspect_elf32(FILE *input)
{
	unsigned char header[52];
	int little;
	const char *endian_name;
	if (fseek(input, 0, SEEK_SET) != 0 || fread(header, 1, sizeof(header), input) != sizeof(header)) {
		fprintf(stderr, "hexscope: file is too short for an ELF32 header\n");
		return -1;
	}
	if (header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' || header[3] != 'F') {
		fprintf(stderr, "hexscope: not an ELF file\n");
		return -1;
	}
	if (header[4] != 1) {
		fprintf(stderr, "hexscope: only ELF32 headers are decoded\n");
		return -1;
	}
	if (header[5] != 1 && header[5] != 2) {
		fprintf(stderr, "hexscope: invalid ELF byte order\n");
		return -1;
	}
	little = header[5] == 1;
	endian_name = "big";
	if (little)
		endian_name = "little";
	printf("ELF32 %s-endian type=%u machine=0x%04x",
	       endian_name,
	       read16(header + 16, little), read16(header + 18, little));
	printf(" entry=0x%08lx", read32(header + 24, little));
	printf(" phoff=0x%08lx", read32(header + 28, little));
	printf(" shoff=0x%08lx\n", read32(header + 32, little));
	if (ferror(stdout))
		return -1;
	return 0;
}
