int main(void) {
    struct { unsigned a:1, b:1; signed n:3; } x = {0};
    x.a = 1;
    x.b = 1;
    x.a = 0;
    x.n = -2;
    return x.a != 0 || x.b != 1 || x.n != -2;
}
