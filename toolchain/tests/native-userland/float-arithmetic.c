int main(void) {
    volatile double a = 1.5, b = 2.25;
    return a + b == 3.75 ? 0 : 1;
}
