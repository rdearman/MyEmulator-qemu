static int check(int count)
{
  int values[count];
  int matrix[count][3];
  for (int i = 0; i < count; i++) {
    values[i] = i + 1;
    matrix[i][2] = 10 + i;
  }
  return sizeof values == count * sizeof(int) &&
    sizeof matrix == count * 3 * sizeof(int) &&
    values[count - 1] == count && matrix[count - 1][2] == 9 + count ? 0 : 1;
}
int main(void) { return check(7); }
