#include "note_store.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
	int result;
	if (argc == 5 && strcmp(argv[1], "add") == 0) {
		result = note_append(argv[2], argv[3], argv[4]);
		if (result < 0) {
			perror(argv[2]);
			return 1;
		}
		return 0;
	}
	if (argc == 3 && strcmp(argv[1], "list") == 0)
		result = note_list(argv[2], 0);
	else if (argc == 4 && strcmp(argv[1], "find") == 0)
		result = note_list(argv[2], argv[3]);
	else {
		fprintf(stderr, "usage: memo add FILE TAG TEXT | list FILE | find FILE TEXT\n");
		return 2;
	}
	if (result < 0) {
		perror("memo");
		return 1;
	}
	return 0;
}
