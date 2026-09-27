unsigned int bitcount32(unsigned int value)
{
	unsigned int count = 0;
	while (value) {
		count += value & 1U;
		value >>= 1;
	}
	return count;
}
