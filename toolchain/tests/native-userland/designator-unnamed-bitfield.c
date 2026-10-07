struct item { int first; unsigned : 32; int last; };
static struct item value = {.last = 42};
int main(void) { return value.last == 42 && value.first == 0 ? 0 : 1; }
