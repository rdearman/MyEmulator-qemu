int first(int), second(int), value;
int third(int), fourth(int);

int first(int n) { return n + 1; }
int second(int n) { return n * 2; }
int third(int n) { return n - 1; }
int fourth(int n) { return n / 2; }

int main(void) {
  value = first(20);
  return second(value) != 42 || third(43) != 42 || fourth(84) != 42;
}
