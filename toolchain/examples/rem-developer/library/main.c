#include <stdio.h>

int rem_add(int left, int right);
int rem_scale(int value, int factor);

int main(void)
{
    int result = rem_scale(rem_add(18, 24), 2);
    puts(result == 84 ? "archive result: 84" : "archive result: incorrect");
    return result != 84;
}
