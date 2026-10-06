static unsigned long long value(int a, int b, int c, unsigned long long d, int e) {
    return a == 1 && b == 2 && c == 3 && e == 4 ? d : 0;
}
int main(void) {
    return value(1, 2, 3, 0x1122334455667788ULL, 4) == 0x1122334455667788ULL ? 0 : 1;
}
