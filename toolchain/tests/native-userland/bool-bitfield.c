int main(void) {
    struct { _Bool a:1, b:1; } x = {0};
    x.a = 1;
    x.b = 0;
    return x.a != 1 || x.b != 0;
}
