#include "note_store.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int write_escaped(FILE *output, const char *text)
{
	while (*text) {
		int c = (unsigned char)*text++;
		if (c == '\\') {
			if (fputs("\\\\", output) == EOF) return -1;
		} else if (c == '\t') {
			if (fputs("\\t", output) == EOF) return -1;
		} else if (c == '\n') {
			if (fputs("\\n", output) == EOF) return -1;
		} else if (c == '\r') {
			if (fputs("\\r", output) == EOF) return -1;
		} else {
			if (fputc(c, output) == EOF) return -1;
		}
	}
	return 0;
}

static int escaped_size(const char *text, size_t limit, size_t *result)
{
	size_t size = 0;
	while (*text) {
		int c = (unsigned char)*text++;
		size_t add = 1;
		if (c == '\\' || c == '\t' || c == '\n' || c == '\r')
			add = 2;
		if (add > limit || size > limit - add)
			return -1;
		size += add;
	}
	*result = size;
	return 0;
}

int note_append(const char *path, const char *tag, const char *text)
{
	FILE *output;
	long end;
	char header[32];
	int last;
	int failed = 0;
	size_t tag_size;
	size_t text_size;
	if (escaped_size(tag, 4093, &tag_size) < 0 ||
	    escaped_size(text, 4093 - tag_size, &text_size) < 0) {
		errno = E2BIG;
		return -1;
	}
	if (tag_size + text_size + 1 > 4094) {
		errno = E2BIG;
		return -1;
	}
	output = fopen(path, "a+");
	if (!output)
		return -1;
	if (fseek(output, 0, SEEK_END) != 0) {
		fclose(output);
		return -1;
	}
	end = ftell(output);
	if (end < 0) {
		fclose(output);
		return -1;
	}
	if (end == 0) {
		if (fputs("REMNOTE1\n", output) == EOF)
			failed = 1;
	} else {
		if (fseek(output, 0, SEEK_SET) != 0 || !fgets(header, sizeof(header), output) ||
		    strcmp(header, "REMNOTE1\n") != 0) {
			fclose(output);
			errno = EINVAL;
			return -1;
		}
		if (end > 9) {
			if (fseek(output, -1L, SEEK_END) != 0 || (last = fgetc(output)) != '\n') {
				fclose(output);
				errno = EINVAL;
				return -1;
			}
		}
		if (fseek(output, 0, SEEK_END) != 0)
			failed = 1;
	}
	if (!failed && (write_escaped(output, tag) < 0 || fputc('\t', output) == EOF ||
			write_escaped(output, text) < 0 || fputc('\n', output) == EOF))
		failed = 1;
	if (fclose(output) != 0)
		failed = 1;
	if (failed)
		return -1;
	return 0;
}

static int write_decoded(FILE *output, const char *start, const char *end)
{
	while (start < end) {
		int c = (unsigned char)*start++;
		if (c == '\\' && start < end) {
			c = (unsigned char)*start++;
			if (c == 't') c = '\t';
			else if (c == 'n') c = '\n';
			else if (c == 'r') c = '\r';
		}
		if (fputc(c, output) == EOF)
			return -1;
	}
	return 0;
}

int note_list(const char *path, const char *needle)
{
	FILE *input = fopen(path, "r");
	char line[4096];
	int failed = 0;
	if (!input)
		return -1;
	if (!fgets(line, sizeof(line), input) || strcmp(line, "REMNOTE1\n") != 0) {
		fclose(input);
		errno = EINVAL;
		return -1;
	}
	while (fgets(line, sizeof(line), input)) {
		char *separator;
		size_t length = strlen(line);
		if (length == sizeof(line) - 1 && line[length - 1] != '\n' && !feof(input)) {
			fprintf(stderr, "memo: record exceeds 4095 bytes\n");
			errno = EFBIG;
			failed = 1;
			break;
		}
		if (length == 0 || line[length - 1] != '\n') {
			fprintf(stderr, "memo: incomplete record at end of file\n");
			errno = EINVAL;
			failed = 1;
			break;
		}
		line[length - 1] = '\0';
		separator = strchr(line, '\t');
		if (!separator) {
			fprintf(stderr, "memo: malformed record\n");
			errno = EINVAL;
			failed = 1;
			break;
		}
		if (!needle || strstr(line, needle)) {
			if (write_decoded(stdout, line, separator) < 0 ||
			    fputs(": ", stdout) == EOF ||
			    write_decoded(stdout, separator + 1, line + strlen(line)) < 0 ||
			    putchar('\n') == EOF)
				failed = 1;
		}
	}
	if (ferror(input))
		failed = 1;
	if (fclose(input) != 0)
		failed = 1;
	if (ferror(stdout))
		failed = 1;
	if (failed)
		return -1;
	return 0;
}
