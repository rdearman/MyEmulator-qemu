static unsigned long long value(unsigned long long n) { return n + 1; }
int main(void) {
    unsigned long long (*call)(unsigned long long) = value;
    volatile unsigned long long n = 0x100000000ULL;
    unsigned long long result = call(n) + n * 3;
    return result == 0x400000001ULL ? 0 : 1;
}
