_Static_assert(sizeof(int) >= 2, "int width");
struct item {
  _Static_assert(1, "structure assertion");
  int value;
};
int main(void)
{
  _Static_assert(sizeof(struct item) == sizeof(int), "item layout");
  return 0;
}
