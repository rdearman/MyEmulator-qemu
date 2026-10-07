#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int lines, words, bytes;
static unsigned long long total_lines, total_words, total_bytes;

static int count_file(const char *name) {
    FILE *file = name && strcmp(name, "-") ? fopen(name, "rb") : stdin;
    if (!file) { perror(name); return 1; }
    unsigned long long l = 0, w = 0, b = 0;
    int inside = 0, c;
    while ((c = fgetc(file)) != EOF) {
        b++;
        if (c == '\n') l++;
        if (isspace((unsigned char)c)) inside = 0;
        else if (!inside) { w++; inside = 1; }
    }
    int result = ferror(file) != 0;
    if (result) perror(name ? name : "stdin");
    if (file != stdin && fclose(file)) { perror(name); result = 1; }
    if (result) return 1;
    total_lines += l;
    total_words += w;
    total_bytes += b;
    if (lines) printf(" %llu", l);
    if (words) printf(" %llu", w);
    if (bytes) printf(" %llu", b);
    if (name) printf(" %s", name);
    putchar('\n');
    return 0;
}

int main(int argc, char **argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (char *p = argv[i] + 1; *p; p++) {
            if (*p == 'l') lines = 1;
            else if (*p == 'w') words = 1;
            else if (*p == 'c') bytes = 1;
            else {
                fprintf(stderr, "bootstrap wc: unsupported option %s\n", argv[i]);
                return 2;
            }
        }
    }
    if (!lines && !words && !bytes) lines = words = bytes = 1;
    if (i == argc) return count_file(0);
    int result = 0, files = argc - i;
    for (; i < argc; i++) if (count_file(argv[i])) result = 1;
    if (files > 1) {
        if (lines) printf(" %llu", total_lines);
        if (words) printf(" %llu", total_words);
        if (bytes) printf(" %llu", total_bytes);
        puts(" total");
    }
    return result;
}
