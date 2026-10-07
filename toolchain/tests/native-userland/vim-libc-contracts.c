#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
  char path[1024];
  char data[16] = "0123456789";
  char name[128];
  struct stat status;
  int fd;
  int i;

  for (i = 0; i < 256; i++)
    if (toupper(i) != (i >= 'a' && i <= 'z' ? i - 'a' + 'A' : i))
      return 1;
  if (!getcwd(path, sizeof(path)) || path[0] != '/')
    return 2;
  memmove(data + 2, data, 8);
  if (memcmp(data, "0101234567", 10))
    return 3;
  memmove(data, data + 2, 8);
  if (memcmp(data, "01234567", 8))
    return 4;
  snprintf(name, sizeof(name), "native-tests/vim-stat-%ld", (long)getpid());
  fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    return 5;
  if (close(fd))
    return 6;
  strcat(name, "/");
  if (stat(name, &status) == 0)
    return 7;
  name[strlen(name) - 1] = 0;
  if (unlink(name))
    return 8;
  puts("VIM_LIBC_CONTRACTS_OK");
  return 0;
}
