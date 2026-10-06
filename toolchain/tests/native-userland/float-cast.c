int main(void) {
    volatile int i = 3;
    volatile double d = (double)i;
    return d == 3.0 && (int)d == i ? 0 : 1;
}
