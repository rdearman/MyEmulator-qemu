extern int helper(int);
extern unsigned data_value;
extern unsigned bss_value;

static int square(int x) { return x * x; }

static int recursive_sum(int n)
{
        if (n <= 0) return 0;
        return n + recursive_sum(n - 1);
}

static unsigned bits(unsigned x)
{
        return (x & 0x55aa55aaU) | ((x ^ 0xffffffffU) & 0x0000ffffU);
}

static int call_through(int (*fn)(int), int x) { return fn(x); }

static int sum12(int a1, int a2, int a3, int a4, int a5, int a6,
                 int a7, int a8, int a9, int a10, int a11, int a12)
{
        return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12;
}

static unsigned inline_add(unsigned a, unsigned b)
{
        unsigned result;
        __asm__ volatile ("add %0, %1, %2"
                          : "=r" (result) : "r" (a), "r" (b));
        return result;
}

static long long wide_mix(long long a, long long b)
{
        return (a * b) + (a << 17) - (b >> 3);
}

int main(void)
{
        int a[4] = { 1, 2, 3, 4 };
        unsigned char bytes[4] = { 0x80, 1, 2, 3 };
        unsigned short half = 0x8001;
        unsigned sum = 0;
        int i;

        for (i = 0; i < 4; ++i) sum += a[i];
        if (sum != 10) return 1;
        if (square(7) != 49) return 2;
        if (recursive_sum(6) != 21) return 3;
        if (call_through(square, 5) != 25) return 4;
        if ((int)bytes[0] != 128 || half != 0x8001) return 5;
        if (bits(0x12345678U) != ((0x12345678U & 0x55aa55aaU) |
                                  (0xedcba987U & 0x0000ffffU))) return 6;
        if (helper(40) != 42 || data_value != 0x12345678U) return 7;
        if (sum12(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) != 78) return 9;
        if (inline_add(0x12340000U, 0x5678U) != 0x12345678U) return 10;
        if (wide_mix(0x100000002LL, 3LL) != 0x2000300040006LL) return 11;
        bss_value = 9;
        if (bss_value != 9) return 8;
        return 0;
}
