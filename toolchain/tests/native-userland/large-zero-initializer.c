int main(void) {
    volatile char values[4096] = {1};
    return values[0] == 1 && values[3000] == 0 && values[4095] == 0 ? 0 : 1;
}
