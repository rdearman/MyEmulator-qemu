int main(void) {
    volatile unsigned long long a = 0x100000010ULL, b = 0xffffffffULL;
    return a - b == 17 ? 0 : 1;
}
