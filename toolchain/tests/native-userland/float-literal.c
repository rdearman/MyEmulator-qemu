int main(void) {
    union { double d; unsigned long long bits; } value;
    value.d = 1.5;
    return value.bits == 0x3ff8000000000000ULL ? 0 : 1;
}
