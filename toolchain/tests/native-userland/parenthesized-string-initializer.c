static const char message[] = ("abc");
int main(void) { return sizeof message != 4 || message[2] != 'c'; }
