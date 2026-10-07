#if __cplusplus >= 201103L
#error undefined __cplusplus incorrectly compares greater than 201103L
#endif
#if 0 >= 201103L
#error zero incorrectly compares greater than 201103L
#endif
int main(void) { return 0; }
