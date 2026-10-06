int main(void) {
    _Alignas(16) char values[16];
    volatile char pad;
    pad = 1;
    return ((unsigned long)(void *)values & 15) == 0 && pad == 1 ? 0 : 1;
}
