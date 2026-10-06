static int aligned(void) {
    _Alignas(16) char values[16];
    return ((unsigned long)(void *)values & 15) == 0;
}
int main(void) {
    int result;
    result = aligned();
    return result ? 0 : 1;
}
