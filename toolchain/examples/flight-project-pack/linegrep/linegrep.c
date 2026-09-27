#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int fold_ascii(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c + ('a' - 'A');
	return c;
}

static int contains_text(const char *line, const char *needle, int ignore_case)
{
	const char *start;
	const char *a;
	const char *b;
	if (*needle == '\0')
		return 1;
	for (start = line; *start; ++start) {
		a = start;
		b = needle;
		while (*a && *b) {
			int left = (unsigned char)*a;
			int right = (unsigned char)*b;
			if (ignore_case) {
				left = fold_ascii(left);
				right = fold_ascii(right);
			}
			if (left != right)
				break;
			++a;
			++b;
		}
		if (*b == '\0')
			return 1;
	}
	return 0;
}

static int print_match(const char *name, unsigned long number, const char *line,
		       int show_number)
{
	if (show_number) {
		printf("%s:%lu:", name, number);
		fputs(line, stdout);
	} else {
		printf("%s:", name);
		fputs(line, stdout);
	}
	if (line[0] && line[strlen(line) - 1] != '\n')
		putchar('\n');
	if (ferror(stdout))
		return -1;
	return 0;
}

static int search_stream(FILE *input, const char *name, const char *needle,
			 int show_number, int ignore_case, int *matched)
{
	size_t capacity = 256;
	size_t used = 0;
	char *line = malloc(capacity);
	int c;
	unsigned long number = 0;
	int result = 0;
	if (!line)
		return -1;
	while ((c = fgetc(input)) != EOF) {
		if (used + 2 > capacity) {
			size_t next = capacity * 2;
			char *grown;
			if (next <= capacity) {
				free(line);
				errno = ENOMEM;
				return -1;
			}
			grown = realloc(line, next);
			if (!grown) {
				free(line);
				return -1;
			}
			line = grown;
			capacity = next;
		}
		line[used++] = (char)c;
		if (c == '\n') {
			line[used] = '\0';
			++number;
			if (contains_text(line, needle, ignore_case)) {
				*matched = 1;
				if (print_match(name, number, line, show_number) < 0)
					result = -1;
			}
			used = 0;
		}
	}
	if (used) {
		line[used] = '\0';
		++number;
		if (contains_text(line, needle, ignore_case)) {
			*matched = 1;
			if (print_match(name, number, line, show_number) < 0)
				result = -1;
		}
	}
	if (ferror(input))
		result = -1;
	free(line);
	return result;
}

int main(int argc, char **argv)
{
	int show_number = 0;
	int ignore_case = 0;
	int arg = 1;
	int matched = 0;
	int had_error = 0;
	const char *needle;
	while (arg < argc && argv[arg][0] == '-' && argv[arg][1]) {
		if (strcmp(argv[arg], "--") == 0) {
			++arg;
			break;
		}
		if (strcmp(argv[arg], "-n") == 0)
			show_number = 1;
		else if (strcmp(argv[arg], "-i") == 0)
			ignore_case = 1;
		else
			break;
		++arg;
	}
	if (arg >= argc) {
		fprintf(stderr, "usage: linegrep [-n] [-i] [--] TEXT [FILE ...]\n");
		return 2;
	}
	needle = argv[arg++];
	if (arg == argc) {
		if (search_stream(stdin, "<stdin>", needle, show_number,
				  ignore_case, &matched) < 0) {
			perror("linegrep");
			return 2;
		}
	} else {
		for (; arg < argc; ++arg) {
			FILE *input;
			const char *name = argv[arg];
			int close_input = 1;
			if (strcmp(name, "-") == 0) {
				input = stdin;
				name = "<stdin>";
				close_input = 0;
			} else {
				input = fopen(name, "r");
				if (!input) {
					perror(name);
					had_error = 1;
					continue;
				}
			}
			if (search_stream(input, name, needle, show_number,
					  ignore_case, &matched) < 0) {
				perror(name);
				had_error = 1;
			}
			if (close_input && fclose(input) != 0) {
				perror(name);
				had_error = 1;
			}
		}
	}
	if (had_error)
		return 2;
	if (matched)
		return 0;
	return 1;
}
