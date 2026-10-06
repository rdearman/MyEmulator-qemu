int main(void) {
    volatile unsigned long long a = 0x100000000ULL;
    if (!a) return 1;
    return a && 1 ? 0 : 2;
}
