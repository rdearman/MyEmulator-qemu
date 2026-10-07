#include <stdio.h>
#include <string.h>
#include <termcap.h>

int main(void) {
  char buffer[2048];
  char *value = tgoto("%p1%d", 0, 1);
  if (!value || strcmp(value, "1"))
    return 1;
  if (tgetent(buffer, "ansi") != 1)
    return 2;
  if (tgetent(buffer, "rem-terminal-that-does-not-exist") != 0)
    return 3;
  puts("VIM_TERMINFO_CONTRACTS_OK");
  return 0;
}
