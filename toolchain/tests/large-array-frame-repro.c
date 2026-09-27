__attribute__((noinline)) int large_array_frame(int value)
{
    volatile unsigned char buffer[4096];
    buffer[0] = (unsigned char)value;
    buffer[4095] = (unsigned char)(value + 1);
    return buffer[0] + buffer[4095];
}

int main(void)
{
    return large_array_frame(20) == 41 ? 0 : 1;
}
