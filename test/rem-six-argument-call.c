/* Runtime regression for the first stack-passed integer arguments.
 * Expected exit status: 0. Return 1 isolates the four-argument control;
 * return 2 means the six-argument result was wrong.
 */
static int sum4(int a, int b, int c, int d)
{
	return a + b + c + d;
}

static int sum6(int a, int b, int c, int d, int e, int f)
{
	return a + b + c + d + e + f;
}

int main(void)
{
	if (sum4(1, 2, 3, 4) != 10)
		return 1;
	if (sum6(1, 2, 3, 4, 5, 6) != 21)
		return 2;
	return 0;
}
