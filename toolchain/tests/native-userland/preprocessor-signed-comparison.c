#if (-1) >= 0
#error "negative preprocessor integer compared as unsigned"
#endif
#if 200809 > 200809L
#error "equal preprocessor integers compare unequal"
#endif
int main(void) { return 0; }
