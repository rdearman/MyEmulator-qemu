/* Regression coverage for the MyEmulator2 GCC frame/LR layout.
   In particular, a callee's saved link register must not overlap the
   caller's outgoing argument area. */

__attribute__((noinline)) static int recursive_sum(int n)
{
    if (n <= 0)
        return 0;
    return n + recursive_sum(n - 1);
}

__attribute__((noinline)) static int sum_twelve(int a1, int a2, int a3,
                                                 int a4, int a5, int a6,
                                                 int a7, int a8, int a9,
                                                 int a10, int a11, int a12)
{
    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12;
}

__attribute__((noinline)) static int add_one(int value)
{
    return value + 1;
}

__attribute__((noinline)) static int indirect_call(int (*fn)(int), int value)
{
    return fn(value);
}

__attribute__((noinline)) static int large_local_frame(int value)
{
    int locals[32];
    int i;
    int total = 0;

    for (i = 0; i < 32; ++i)
        locals[i] = value + i;
    for (i = 0; i < 32; ++i)
        total += locals[i];
    return total;
}

int main(void)
{
    if (recursive_sum(7) != 28)
        return 1;
    if (sum_twelve(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) != 78)
        return 2;
    if (indirect_call(add_one, 41) != 42)
        return 3;
    if (large_local_frame(3) != 3 * 32 + (31 * 32) / 2)
        return 4;
    return 0;
}
