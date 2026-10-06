int main(void) {
    volatile long long a = 1;
    volatile long long b = 0x100000000LL;
    return -a == -1 && -b == -0x100000000LL ? 0 : 1;
}
