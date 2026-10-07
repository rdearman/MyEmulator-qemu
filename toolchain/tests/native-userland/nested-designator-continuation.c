struct pair { int first, last; };
static struct pair items[2] = {
  [0].first = 1, [0].last = 2, [1].first = 3, [1].last = 4
};
static struct pair continuation = {.first = 11, 22};
int main(void)
{
  return items[0].first == 1 && items[0].last == 2 &&
         items[1].first == 3 && items[1].last == 4 &&
         continuation.first == 11 && continuation.last == 22 ? 0 : 1;
}
