int main(void) {
    volatile long long a = 0x180000000LL, b = 0x100000001LL;
    volatile long long c = 0x100000001LL;
    return !(a < b) && c <= b ? 0 : 1;
}
