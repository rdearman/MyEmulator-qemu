struct small { int first, last; };

struct small small_copy(struct small value)
{
  value.last += 1;
  return value;
}

int main(void)
{
  struct small s = {11, 22};
  struct small t = small_copy(s);
  if (s.first != 11 || s.last != 22) return 1;
  if (t.first != 11) return 2;
  if (t.last != 23) return 3;
  return 0;
}
