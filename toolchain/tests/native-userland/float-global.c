static double number = 1.5;
int main(void) {
    union { double d; unsigned long long bits; } value;
    value.d = number;
    return value.bits == 0x3ff8000000000000ULL ? 0 : 1;
}
