#include "chibicc.h"
_Noreturn void error(char *fmt, ...)
{
  (void)fmt;
  abort();
}
int main(void)
{
  HashMap map = {};
  hashmap_put(&map, "I", (void *)1);
  hashmap_put(&map, "Y", (void *)2);
  hashmap_delete(&map, "I");
  hashmap_put(&map, "Y", (void *)3);
  int count = 0;
  for (int i = 0; i < map.capacity; i++)
    if (map.buckets[i].key && map.buckets[i].key != (void *)-1)
      count++;
  return count != 1 || hashmap_get(&map, "Y") != (void *)3;
}
