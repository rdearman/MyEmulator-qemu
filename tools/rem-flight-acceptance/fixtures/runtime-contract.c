#include <stdarg.h>

int puts(const char *);

static int text_equal(const char *left, const char *right)
{
    while (*left && *right && *left == *right) {
        left++;
        right++;
    }
    return *left == *right;
}

static int add_variadic(int count, ...)
{
    va_list args;
    int total = 0;

    va_start(args, count);
    while (count-- > 0)
        total += va_arg(args, int);
    va_end(args);
    return total;
}

int main(int argc, char **argv)
{
    const char *const words[] = { "table-zero", "table-one", "table-two" };
    unsigned long long high = 0x0000000100000001ULL;
    unsigned long long low = 0x0000000200000003ULL;

    if (argc != 3 || !text_equal(argv[1], "alpha") || !text_equal(argv[2], "beta"))
        return 11;
    puts("RUNTIME_PASS_ARGV");

    if (add_variadic(4, 10, 11, 12, 9) != 42)
        return 12;
    puts("RUNTIME_PASS_VARIADIC");

    if (!text_equal(words[0], "table-zero") ||
        !text_equal(words[1], "table-one") ||
        !text_equal(words[2], "table-two"))
        return 13;
    puts("RUNTIME_PASS_POINTERS");

    if (high + low != 0x0000000300000004ULL ||
        low - high != 0x0000000100000002ULL ||
        (high << 1) != 0x0000000200000002ULL)
        return 14;
    puts("RUNTIME_PASS_INT64");
    return 0;
}
