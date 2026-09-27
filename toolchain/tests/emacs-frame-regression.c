/*
 * Reproducer for the REM GCC large-frame caller-frame restoration defect.
 * The outer frame is read after a call to a function with a 1104-byte local.
 * Expected process return value: 0.
 */
int step(int x)
{
    return x + 3;
}

int large_callee(int x)
{
    volatile int frame[276]; /* 1104 bytes */
    frame[0] = x;
    frame[275] = step(x);
    return frame[0] + frame[275];
}

int outer(int x)
{
    volatile int saved[8];
    saved[0] = x;
    int y = large_callee(x);
    return y + saved[0];
}

int main(void)
{
    return outer(19) == 60 ? 0 : 1;
}
